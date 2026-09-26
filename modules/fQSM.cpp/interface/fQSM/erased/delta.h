#pragma once

#include <cstddef>
#include <cstdint>

#include <fQSM/erased/line.h>
#include <fQSM/erased/patch_line.h>

namespace fqsm::erased {

    enum class DeltaMode : std::uint8_t { clean, dirty };
    enum class DeltaLayer : std::uint8_t { all, added, addedOrUpdated, removed, updated };

    // The mode a reader of one layer asks for. A tainted entry (mutated in place, no value before) matches only
    // the all and addedOrUpdated layers; for the other layers the dirty walk over the whole line yields what the
    // patch walk yields, so a reader of those layers asks for the clean mode and pays for the patch, not the line.
    constexpr DeltaMode reading_mode(DeltaMode lineMode, DeltaLayer layer) {
        const bool seesTainted = layer == DeltaLayer::all or layer == DeltaLayer::addedOrUpdated;
        return seesTainted ? lineMode : DeltaMode::clean;
    }

    // tainted: the value was mutated in place (dirty mode), so the value before is unknown.
    struct Change {
        RawId id;
        const void* before;
        const void* after;
        bool tainted;

        bool good() const { return not tainted or after; }
        bool add() const { return not tainted and not before and after; }
        bool update() const { return not tainted and before and after; }
        bool remove() const { return not tainted and before and not after; }
        bool addedOrUpdated() const { return add() or update() or (tainted and after); }
    };

    // Clean mode walks the patch only, taking before from state.
    // Dirty mode walks state first (patched ids as usual, other ids as tainted), then patch-only ids.
    class DeltaCursor {
    public:
        DeltaCursor() = default;

        static DeltaCursor begin(const ReadLine& state, const PatchLine& patch, DeltaMode mode, DeltaLayer layer);
        static DeltaCursor end(const ReadLine& state, const PatchLine& patch, DeltaMode mode, DeltaLayer layer);

        Change operator*() const;
        DeltaCursor& operator++();
        bool operator==(const DeltaCursor& other) const;

    private:
        static bool matches(DeltaLayer layer, const Change& change);
        void skip_to_match();
        bool in_state_phase() const { return mode == DeltaMode::dirty and not (stateCursor == stateEnd); }
        Change from_state() const;
        Change from_patch() const;

        const ReadLine* state = nullptr;
        const PatchLine* patch = nullptr;
        Cursor stateCursor{};
        Cursor stateEnd{};
        std::size_t patchIndex = 0;
        std::size_t patchCount = 0;
        DeltaLayer layer = DeltaLayer::all;
        DeltaMode mode = DeltaMode::clean;
    };

    bool delta_empty(const ReadLine& state, const PatchLine& patch, DeltaMode mode, DeltaLayer layer);
}
