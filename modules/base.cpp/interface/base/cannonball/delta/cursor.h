#pragma once

#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>

#include <base/cannonball/cursor.h>
#include <base/cannonball/delta/changesLanguage.h>
#include <base/cannonball/patchlet.h>
#include <base/cannonball/table/read.h>

namespace base::cannonball::delta {

enum class Mode { clean, dirty };
enum class Layer { all, added, addedOrUpdated, removed, updated };

// Concrete delta cursor: clean mode walks the patch only (before from state.find);
// dirty mode walks state (producing dirty-state values) then patch-only entries not in state.
template<typename Key, typename Val>
class DeltaCursor {
public:
    using ChangeType = Change<Key, Val>;
    using StateView = table::Read<Key, Val>;
    using PatchView = table::Read<Key, Patchlet<Val>>;
    using StateCursor = base::cannonball::Cursor<Key, Val>;
    using PatchCursor = base::cannonball::Cursor<Key, Patchlet<Val>>;

    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = ChangeType;
    using pointer = const value_type*;
    using reference = const value_type&;

    DeltaCursor() = default;

    static DeltaCursor clean_at(
        const StateView& state, const PatchView& patch,
        PatchCursor patchCursor, PatchCursor patchEnd, Layer layer)
    {
        DeltaCursor cursor;
        cursor.mode_ = Mode::clean;
        cursor.state = std::addressof(state);
        cursor.patch = std::addressof(patch);
        cursor.patchCursor = std::move(patchCursor);
        cursor.patchEnd = std::move(patchEnd);
        cursor.layer = layer;
        cursor.skip_to_match();
        return cursor;
    }

    static DeltaCursor dirty_at(
        const StateView& state, const PatchView& patch,
        StateCursor stateCursor, StateCursor stateEnd,
        PatchCursor patchCursor, PatchCursor patchEnd, Layer layer)
    {
        DeltaCursor cursor;
        cursor.mode_ = Mode::dirty;
        cursor.state = std::addressof(state);
        cursor.patch = std::addressof(patch);
        cursor.stateCursor = std::move(stateCursor);
        cursor.stateEnd = std::move(stateEnd);
        cursor.patchCursor = std::move(patchCursor);
        cursor.patchEnd = std::move(patchEnd);
        cursor.layer = layer;
        cursor.skip_to_match();
        return cursor;
    }

    value_type operator*() const { return dereference(); }

    struct ArrowProxy {
        value_type view;
        const value_type* operator->() const { return &view; }
    };

    ArrowProxy operator->() const { return ArrowProxy{dereference()}; }

    DeltaCursor& operator++() {
        if (mode_ == Mode::dirty && stateCursor != stateEnd) ++stateCursor;
        else ++patchCursor;

        skip_to_match();
        return *this;
    }

    DeltaCursor operator++(int) {
        DeltaCursor copy = *this;
        ++*this;
        return copy;
    }

    bool operator==(const DeltaCursor& other) const {
        if (mode_ != other.mode_) return false;
        if (state != other.state || patch != other.patch) return false;
        if (mode_ == Mode::dirty && stateCursor != other.stateCursor) return false;
        return patchCursor == other.patchCursor;
    }

    bool operator!=(const DeltaCursor& other) const { return !(*this == other); }

private:
    static bool matches(Layer layer, const ChangeType& change) {
        if (layer == Layer::all) return change.good();
        if (layer == Layer::added) return change.add();
        if (layer == Layer::addedOrUpdated) return change.addedOrUpdated();
        if (layer == Layer::removed) return change.remove();
        if (layer == Layer::updated) return change.update();
        return false;
    }

    void skip_to_match() {
        if (mode_ == Mode::dirty) {
            while (stateCursor != stateEnd) {
                if (matches(layer, make_dirty_state_value())) return;
                ++stateCursor;
            }
        }

        while (patchCursor != patchEnd) {
            if (mode_ == Mode::dirty && state->contains((*patchCursor).id)) {
                ++patchCursor;
                continue;
            }
            if (matches(layer, make_from_patch_entry())) return;
            ++patchCursor;
        }
    }

    ChangeType make_dirty_state_value() const {
        const auto entry = *stateCursor;
        if (const auto* patchEntry = patch->find(entry.id)) {
            const auto* after = patchEntry->tombstone ? nullptr : std::addressof(patchEntry->quantum);
            return ChangeType{entry.id, std::optional<const Val*>{std::addressof(entry.value)}, after};
        }
        return ChangeType{entry.id, std::nullopt, std::addressof(entry.value)};
    }

    ChangeType make_from_patch_entry() const {
        const auto entry = *patchCursor;
        const auto* before = state->find(entry.id);
        const auto* after = entry.value.tombstone ? nullptr : std::addressof(entry.value.quantum);
        return ChangeType{entry.id, std::optional<const Val*>{before}, after};
    }

    ChangeType dereference() const {
        if (mode_ == Mode::dirty && stateCursor != stateEnd) return make_dirty_state_value();
        return make_from_patch_entry();
    }

    const StateView* state = nullptr;
    const PatchView* patch = nullptr;
    StateCursor stateCursor{};
    StateCursor stateEnd{};
    PatchCursor patchCursor{};
    PatchCursor patchEnd{};
    Layer layer = Layer::all;
    Mode mode_ = Mode::clean;
};

} // namespace base::cannonball::delta
