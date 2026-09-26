#include <fQSM/erased/delta.h>

namespace fqsm::erased {

    DeltaCursor DeltaCursor::begin(const ReadLine& state, const PatchLine& patch, DeltaMode mode, DeltaLayer layer) {
        DeltaCursor cursor;
        cursor.state = &state;
        cursor.patch = &patch;
        cursor.mode = mode;
        cursor.layer = layer;
        cursor.patchCount = patch.count();
        if (mode == DeltaMode::dirty) {
            cursor.stateCursor = state.cursor_begin();
            cursor.stateEnd = state.cursor_end();
        }
        cursor.skip_to_match();
        return cursor;
    }

    DeltaCursor DeltaCursor::end(const ReadLine& state, const PatchLine& patch, DeltaMode mode, DeltaLayer layer) {
        DeltaCursor cursor;
        cursor.state = &state;
        cursor.patch = &patch;
        cursor.mode = mode;
        cursor.layer = layer;
        cursor.patchCount = patch.count();
        cursor.patchIndex = cursor.patchCount;
        if (mode == DeltaMode::dirty) {
            cursor.stateCursor = state.cursor_end();
            cursor.stateEnd = cursor.stateCursor;
        }
        return cursor;
    }

    Change DeltaCursor::operator*() const {
        return in_state_phase() ? from_state() : from_patch();
    }

    DeltaCursor& DeltaCursor::operator++() {
        if (in_state_phase()) ++stateCursor;
        else ++patchIndex;
        skip_to_match();
        return *this;
    }

    bool DeltaCursor::operator==(const DeltaCursor& other) const {
        if (mode != other.mode) return false;
        if (state != other.state or patch != other.patch) return false;
        if (mode == DeltaMode::dirty and not (stateCursor == other.stateCursor)) return false;
        return patchIndex == other.patchIndex;
    }

    bool DeltaCursor::matches(DeltaLayer layer, const Change& change) {
        switch (layer) {
        case DeltaLayer::all: return change.good();
        case DeltaLayer::added: return change.add();
        case DeltaLayer::addedOrUpdated: return change.addedOrUpdated();
        case DeltaLayer::removed: return change.remove();
        case DeltaLayer::updated: return change.update();
        }
        return false;
    }

    void DeltaCursor::skip_to_match() {
        if (mode == DeltaMode::dirty) {
            while (not (stateCursor == stateEnd)) {
                if (matches(layer, from_state())) return;
                ++stateCursor;
            }
        }
        while (patchIndex < patchCount) {
            if (mode == DeltaMode::dirty and state->contains(patch->id_at(patchIndex))) {
                ++patchIndex;
                continue;
            }
            if (matches(layer, from_patch())) return;
            ++patchIndex;
        }
    }

    Change DeltaCursor::from_state() const {
        const auto entry = *stateCursor;
        if (const auto found = patch->mention(entry.id); found.found)
            return Change{entry.id, entry.value, found.tombstone ? nullptr : found.value, false};
        return Change{entry.id, nullptr, entry.value, true};
    }

    Change DeltaCursor::from_patch() const {
        const RawId id = patch->id_at(patchIndex);
        const auto found = patch->at(patchIndex);
        return Change{id, state->find(id), found.tombstone ? nullptr : found.value, false};
    }

    bool delta_empty(const ReadLine& state, const PatchLine& patch, DeltaMode mode, DeltaLayer layer) {
        return DeltaCursor::begin(state, patch, mode, layer) == DeltaCursor::end(state, patch, mode, layer);
    }
}
