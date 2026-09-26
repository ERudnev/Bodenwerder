#include <fQSM/erased/line.h>
#include <fQSM/erased/patch_line.h>

#include <cassert>
#include <cstdlib>

namespace fqsm::erased {

    // Cursor

    Cursor Cursor::plain(const Line& line, std::size_t position) {
        Cursor cursor;
        cursor.root = &line;
        cursor.rootIndex = position;
        return cursor;
    }

    Cursor Cursor::overlay(const Cursor& base, const PatchLine& layer, bool atEnd) {
        assert(base.layerCount < MaxLayers && "fqsm::erased::Cursor: layer capacity exceeded");
        if (base.layerCount >= MaxLayers) std::abort();

        Cursor cursor;
        cursor.root = base.root;
        cursor.layerCount = base.layerCount;
        for (std::size_t i = 0; i < base.layerCount; ++i) cursor.layers[i] = base.layers[i];
        cursor.layers[cursor.layerCount++] = &layer;

        if (atEnd) {
            cursor.phase = Phase::end;
            cursor.rootIndex = cursor.root ? cursor.root->size() : 0;
            cursor.currentLayer = cursor.layerCount;
        } else {
            cursor.skip_to_visible();
        }
        return cursor;
    }

    auto Cursor::operator*() const -> Entry {
        if (layerCount == 0)
            return Entry{root->id_at(rootIndex), root->value_at(rootIndex)};

        if (phase == Phase::root) {
            const RawId id = root->id_at(rootIndex);
            if (const auto found = mention(id); found.found) return Entry{id, found.value};
            return Entry{id, root->value_at(rootIndex)};
        }

        // currentLayer itself mentions the id, so the topmost mention always exists
        const RawId id = layers[currentLayer]->id_at(layerIndex);
        return Entry{id, mention(id).value};
    }

    Cursor& Cursor::operator++() {
        if (layerCount == 0) {
            ++rootIndex;
            return *this;
        }
        if (phase == Phase::root) ++rootIndex;
        else if (phase == Phase::layer) ++layerIndex;
        skip_to_visible();
        return *this;
    }

    bool Cursor::operator==(const Cursor& other) const {
        if (layerCount != other.layerCount) return false;
        if (root != other.root) return false;
        if (layerCount == 0) return rootIndex == other.rootIndex;

        for (std::size_t i = 0; i < layerCount; ++i)
            if (layers[i] != other.layers[i]) return false;

        if (phase != other.phase) return false;
        if (phase == Phase::root) return rootIndex == other.rootIndex;
        if (phase == Phase::layer) return currentLayer == other.currentLayer and layerIndex == other.layerIndex;
        return true;
    }

    Mention Cursor::mention(RawId id) const {
        return mention_below(layerCount, id);
    }

    Mention Cursor::mention_below(std::size_t layer, RawId id) const {
        for (std::size_t i = layer; i-- > 0; )
            if (const auto found = layers[i]->mention(id); found.found) return found;
        return {};
    }

    bool Cursor::visible_after_all(RawId id) const {
        if (const auto found = mention(id); found.found) return not found.tombstone;
        return root->contains(id);
    }

    bool Cursor::visible_below(std::size_t layer, RawId id) const {
        if (const auto found = mention_below(layer, id); found.found) return not found.tombstone;
        return root->contains(id);
    }

    void Cursor::skip_to_visible() {
        if (phase == Phase::end) return;

        if (phase == Phase::root) {
            while (rootIndex < root->size()) {
                if (visible_after_all(root->id_at(rootIndex))) return;
                ++rootIndex;
            }
            phase = Phase::layer;
            currentLayer = 0;
            layerIndex = 0;
        }

        while (currentLayer < layerCount) {
            const PatchLine& layer = *layers[currentLayer];
            const std::size_t count = layer.count();
            while (layerIndex < count) {
                const RawId id = layer.id_at(layerIndex);
                if (not layer.at(layerIndex).tombstone
                    and not root->contains(id)
                    and not visible_below(currentLayer, id)
                    and visible_after_all(id))
                    return;
                ++layerIndex;
            }
            ++currentLayer;
            layerIndex = 0;
        }

        phase = Phase::end;
    }

    // Line

    Line::Line(const Ops& quantum, const Ops& global, GlobalStart start)
        : slots(quantum)
        , globalSlot(global)
    {
        if (start == GlobalStart::constructed and global.construct)
            globalSlot.push_default();
    }

    Line::Line(const Descriptor& descriptor)
        : Line(*descriptor.quantum, *descriptor.global)
    {}

    bool Line::contains(RawId id) const {
        return positions.contains(id);
    }

    const void* Line::find(RawId id) const {
        const auto found = positions.find(id);
        if (found == positions.end()) return nullptr;
        return slots.at(found->second);
    }

    void* Line::find_mutable(RawId id) {
        const auto found = positions.find(id);
        if (found == positions.end()) return nullptr;
        return slots.at(found->second);
    }

    const void* Line::global() const {
        return globalSlot.empty() ? nullptr : globalSlot.at(0);
    }

    void* Line::global_mutable() {
        return globalSlot.empty() ? nullptr : globalSlot.at(0);
    }

    void Line::set_global(const void* value) {
        globalSlot.push_copy(value);
        if (globalSlot.size() > 1)
            globalSlot.release(0);
    }

    void Line::reset_global() {
        globalSlot.clear();
    }

    std::size_t Line::position_of(RawId id) const {
        const auto found = positions.find(id);
        return found == positions.end() ? npos : found->second;
    }

    Cursor Line::cursor_begin() const {
        return Cursor::plain(*this, 0);
    }

    Cursor Line::cursor_end() const {
        return Cursor::plain(*this, ids.size());
    }

    // The fresh value is fully built at the back before an old value for the same id is destroyed.
    void* Line::place(RawId id, Slots::Index fresh) {
        const auto found = positions.find(id);
        if (found != positions.end()) {
            const Slots::Index slot = found->second;
            slots.release(slot);
            return slots.at(slot);
        }
        try {
            ids.push_back(id);
            positions.emplace(id, fresh);
        } catch (...) {
            if (ids.size() > fresh) ids.pop_back();
            slots.release(fresh);
            throw;
        }
        return slots.at(fresh);
    }

    void* Line::insert(RawId id, const void* value) {
        return place(id, slots.push_copy(value));
    }

    void* Line::emplace_move(RawId id, void* value) {
        return place(id, slots.push_move(value));
    }

    bool Line::erase(RawId id) {
        const auto found = positions.find(id);
        if (found == positions.end()) return false;

        const Slots::Index slot = found->second;
        positions.erase(found);
        if (slots.release(slot)) {
            ids[slot] = ids.back();
            positions[ids[slot]] = slot;
        }
        ids.pop_back();
        return true;
    }

    void Line::clear() {
        ids.clear();
        positions.clear();
        slots.clear();
    }

    void Line::reserve(std::size_t capacity) {
        ids.reserve(capacity);
        positions.reserve(capacity);
        slots.reserve(capacity);
    }

    void Line::clone(const ReadLine& source) {
        if (&source == this) return;
        clear();
        reserve(source.size());
        for (auto it = source.cursor_begin(), end = source.cursor_end(); not (it == end); ++it) {
            const auto entry = *it;
            insert(entry.id, entry.value);
        }
        if (const void* global = source.global())
            set_global(global);
    }
}
