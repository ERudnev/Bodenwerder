#include <fQSM/erased/future_line.h>

namespace fqsm::erased {

    bool FutureLine::contains(RawId id) const {
        return find(id) != nullptr;
    }

    const void* FutureLine::find(RawId id) const {
        if (const auto found = layer->mention(id); found.found)
            return found.tombstone ? nullptr : found.value;
        return below->find(id);
    }

    std::size_t FutureLine::size() const {
        std::size_t result = below->size();
        const std::size_t count = layer->count();
        for (std::size_t i = 0; i < count; ++i) {
            const bool existed = below->contains(layer->id_at(i));
            if (layer->at(i).tombstone) {
                if (existed) --result;
                continue;
            }
            if (not existed) ++result;
        }
        return result;
    }

    const void* FutureLine::global() const {
        if (const void* patched = layer->global()) return patched;
        return below->global();
    }

    Cursor FutureLine::cursor_begin() const {
        return Cursor::overlay(below->cursor_begin(), *layer, false);
    }

    Cursor FutureLine::cursor_end() const {
        return Cursor::overlay(below->cursor_end(), *layer, true);
    }

    void FutureLine::put_modification(RawId id, void* value) {
        layer->modify_move(id, value);
    }

    void FutureLine::put_add(RawId id, void* value) {
        layer->modify_move(id, value);
    }

    void FutureLine::put_global(const void* value) {
        layer->set_global(value);
    }

    void FutureLine::put_deletion(RawId id) {
        if (const auto patched = layer->mention(id); patched.found) {
            layer->del(id, patched.value);
            return;
        }
        if (const void* current = below->find(id))
            layer->del(id, current);
    }

    void* FutureLine::get_modification_access(RawId id) {
        if (void* patched = layer->find_mutable(id))
            return patched;
        const void* current = below->find(id);
        if (not current) return nullptr;
        return layer->touch(id, current);
    }

    void* FutureLine::get_access_global() {
        return layer->touch_global(below->global());
    }
}
