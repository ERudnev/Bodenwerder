#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <vector>

#include <base/cannonball/entry.h>
#include <base/cannonball/patchlet.h>

namespace base::cannonball::table {

template<typename Key, typename Val>
class Read;

} // namespace base::cannonball::table

namespace base::cannonball {

// Forward (non-owning) read cursor over a root vector plus a stack of overlaid patch layers.
// Plain mode (layerCount == 0) walks the root vector directly, no filtering: this is Table's
// own iteration. Overlay mode (Future) composes layers 0..layerCount-1, oldest (0) to topmost
// (layerCount-1): for a given id, the topmost layer that mentions it decides visibility (a
// tombstone hides it, otherwise it supplies the value); falling back to root membership when no
// layer mentions the id. Root entries are always emitted in the root phase (using the topmost
// mention's value, if any); a patch-only id is emitted exactly once, by the lowest layer at which
// it first becomes visible. See overlay() for how one more layer is composed onto an existing
// (possibly already-layered) cursor — this is what makes Future-over-Future-over-... work.
template<typename Key, typename Val>
class Cursor {
public:
    using SizeType = std::size_t;
    using StateVector = std::vector<Entry<Key, Val>>;
    using PatchVector = std::vector<Entry<Key, Patchlet<Val>>>;
    using StateView = table::Read<Key, Val>;
    using PatchView = table::Read<Key, Patchlet<Val>>;

    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = EntryView<Key, Val>;
    using pointer = const value_type*;
    using reference = const value_type&;

    // Fixed nesting capacity. Normalization nests three Futures deep (advancing, proposal,
    // Review's inner Operational); each nested establish::Branch adds one more. Exceeding this
    // is a logic bug (pathological nesting), not a recoverable runtime condition: see overlay().
    static constexpr SizeType MaxLayers = 8;

    struct Layer {
        const PatchVector* entries = nullptr;
        const PatchView* view = nullptr;
    };

    enum class Phase { root, layer, end };

    Cursor() = default;

    static Cursor plain(const StateVector* entries, SizeType index, const StateView* rootView) {
        Cursor cursor;
        cursor.root = entries;
        cursor.rootIndex = index;
        cursor.rootView = rootView;
        return cursor;
    }

    // Copies base's layer stack — whatever its depth, including zero (plain) — and pushes one
    // more layer on top, then resolves the result's traversal position from scratch (root phase,
    // index 0; or the end sentinel, per atEnd) using the full, extended stack. base's own
    // traversal position is irrelevant: visibility under N+1 layers can differ from visibility
    // under N, so resuming from a position resolved with fewer layers would be wrong (an entry
    // base already skipped past might become visible again under the new top layer).
    static Cursor overlay(const Cursor& base, const PatchVector* patchEntries, const PatchView* patchView, bool atEnd) {
        assert(base.layerCount < MaxLayers && "cannonball::Cursor: layer capacity exceeded (see MaxLayers)");
        if (base.layerCount >= MaxLayers) std::abort();

        Cursor cursor;
        cursor.root = base.root;
        cursor.rootView = base.rootView;
        cursor.layerCount = base.layerCount;
        for (SizeType i = 0; i < base.layerCount; ++i) cursor.layers[i] = base.layers[i];
        cursor.layers[cursor.layerCount] = Layer{patchEntries, patchView};
        ++cursor.layerCount;

        if (atEnd) {
            cursor.phase = Phase::end;
            cursor.rootIndex = cursor.root ? cursor.root->size() : 0;
            cursor.currentLayer = cursor.layerCount;
        } else {
            cursor.phase = Phase::root;
            cursor.rootIndex = 0;
            cursor.currentLayer = 0;
            cursor.layerIndex = 0;
            cursor.skip_to_visible();
        }

        return cursor;
    }

    value_type operator*() const {
        if (layerCount == 0) {
            const auto& entry = (*root)[rootIndex];
            return value_type{entry.id, entry.value};
        }

        if (phase == Phase::root) {
            const auto& entry = (*root)[rootIndex];
            if (const auto* found = mention(entry.id)) return value_type{entry.id, found->quantum};
            return value_type{entry.id, entry.value};
        }

        const auto& entry = (*layers[currentLayer].entries)[layerIndex];
        const auto* found = mention(entry.id); // always non-null: currentLayer itself mentions entry.id
        return value_type{entry.id, found->quantum};
    }

    struct ArrowProxy {
        value_type view;
        const value_type* operator->() const { return &view; }
    };

    ArrowProxy operator->() const { return ArrowProxy{**this}; }

    Cursor& operator++() {
        if (layerCount == 0) {
            ++rootIndex;
            return *this;
        }

        if (phase == Phase::root) ++rootIndex;
        else if (phase == Phase::layer) ++layerIndex;

        skip_to_visible();
        return *this;
    }

    Cursor operator++(int) {
        Cursor copy = *this;
        ++*this;
        return copy;
    }

    bool operator==(const Cursor& other) const {
        if (layerCount != other.layerCount) return false;
        if (root != other.root) return false;
        if (layerCount == 0) return rootIndex == other.rootIndex;

        for (SizeType i = 0; i < layerCount; ++i)
            if (layers[i].entries != other.layers[i].entries) return false;

        if (phase != other.phase) return false;
        if (phase == Phase::root) return rootIndex == other.rootIndex;
        if (phase == Phase::layer) return currentLayer == other.currentLayer && layerIndex == other.layerIndex;
        return true; // both Phase::end
    }

    bool operator!=(const Cursor& other) const { return !(*this == other); }

private:
    // Topmost (highest-index) layer mentioning id, or null if none does.
    const Patchlet<Val>* mention(const Key& id) const {
        for (SizeType i = layerCount; i-- > 0; )
            if (const auto* found = layers[i].view->find(id)) return found;
        return nullptr;
    }

    // Same, restricted to layers below k (indices 0..k-1).
    const Patchlet<Val>* mention_below(SizeType k, const Key& id) const {
        for (SizeType i = k; i-- > 0; )
            if (const auto* found = layers[i].view->find(id)) return found;
        return nullptr;
    }

    bool visible_after_all(const Key& id) const {
        if (const auto* found = mention(id)) return !found->tombstone;
        return rootView->contains(id);
    }

    bool visible_below(SizeType k, const Key& id) const {
        if (const auto* found = mention_below(k, id)) return !found->tombstone;
        return rootView->contains(id);
    }

    void skip_to_visible() {
        if (phase == Phase::end) return;

        if (phase == Phase::root) {
            while (rootIndex < root->size()) {
                const auto& entry = (*root)[rootIndex];
                if (visible_after_all(entry.id)) return;
                ++rootIndex;
            }
            phase = Phase::layer;
            currentLayer = 0;
            layerIndex = 0;
        }

        while (currentLayer < layerCount) {
            const auto& entries = *layers[currentLayer].entries;
            while (layerIndex < entries.size()) {
                const auto& entry = entries[layerIndex];
                if (!entry.value.tombstone
                    && !rootView->contains(entry.id)
                    && !visible_below(currentLayer, entry.id)
                    && visible_after_all(entry.id))
                    return;
                ++layerIndex;
            }
            ++currentLayer;
            layerIndex = 0;
        }

        phase = Phase::end;
    }

    const StateVector* root = nullptr;
    SizeType rootIndex = 0;
    const StateView* rootView = nullptr;

    Layer layers[MaxLayers]{};
    SizeType layerCount = 0;

    Phase phase = Phase::root;
    SizeType currentLayer = 0;
    SizeType layerIndex = 0;
};

// Forward (non-owning) mutable cursor over a plain entries vector; replaces Direct's WriteIterator.
template<typename Key, typename Val>
class MutableCursor {
public:
    using SizeType = std::size_t;
    using StateVector = std::vector<Entry<Key, Val>>;

    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = EntryRef<Key, Val>;
    using pointer = value_type*;
    using reference = value_type&;

    MutableCursor() = default;

    MutableCursor(StateVector* entries, SizeType index)
        : entries(entries)
        , index(index)
    {}

    value_type operator*() const {
        auto& entry = (*entries)[index];
        return value_type{entry.id, entry.value};
    }

    struct ArrowProxy {
        value_type view;
        const value_type* operator->() const { return &view; }
    };

    ArrowProxy operator->() const { return ArrowProxy{**this}; }

    MutableCursor& operator++() {
        ++index;
        return *this;
    }

    MutableCursor operator++(int) {
        MutableCursor copy = *this;
        ++*this;
        return copy;
    }

    bool operator==(const MutableCursor& other) const {
        return entries == other.entries && index == other.index;
    }

    bool operator!=(const MutableCursor& other) const { return !(*this == other); }

private:
    StateVector* entries = nullptr;
    SizeType index = 0;
};

} // namespace base::cannonball
