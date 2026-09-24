#include <fQSM/erased/overlay.h>

namespace fqsm::erased {

    bool Overlay::contains(RawId id) const {
        return find(id) != nullptr;
    }

    const void* Overlay::find(RawId id) const {
        if (const auto found = layer->mention(id); found.found)
            return found.tombstone ? nullptr : found.value;
        return below->find(id);
    }

    std::size_t Overlay::size() const {
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

    const void* Overlay::global() const {
        if (const void* patched = layer->global()) return patched;
        return below->global();
    }

    Cursor Overlay::cursor_begin() const {
        return Cursor::overlay(below->cursor_begin(), *layer, false);
    }

    Cursor Overlay::cursor_end() const {
        return Cursor::overlay(below->cursor_end(), *layer, true);
    }
}
