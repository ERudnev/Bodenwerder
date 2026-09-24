#include <fQSM/model/complex/future.h>

namespace fqsm::model::complex {

    Future::Future(const State& state, ref<Patch> patch, const Rtid::Set& dirty)
        : State(state.schema)
        , state(state)
        , changes(std::move(patch))
        , dirty(dirty)
        , lines(state.schema->slotCount())
    {}

    erased::FutureLine& Future::future(Slot slot) const {
        auto& line = lines[slot];
        if (not line)
            line = std::make_unique<erased::FutureLine>(state.line(slot), changes->writable(slot));
        return *line;
    }

    const erased::PatchLine& Future::patch_line(Slot slot) const {
        const auto* line = changes->line(slot);
        return line ? *line : erased::empty_patch_line();
    }

    erased::DeltaMode Future::delta_mode(Slot slot) const {
        return dirty.contains(schema->descriptors[slot].id) ? erased::DeltaMode::dirty : erased::DeltaMode::clean;
    }

    erased::DeltaCursor Future::delta_begin(Slot slot, erased::DeltaLayer layer) const {
        return erased::DeltaCursor::begin(state.line(slot), patch_line(slot), delta_mode(slot), layer);
    }

    erased::DeltaCursor Future::delta_end(Slot slot, erased::DeltaLayer layer) const {
        return erased::DeltaCursor::end(state.line(slot), patch_line(slot), delta_mode(slot), layer);
    }
}
