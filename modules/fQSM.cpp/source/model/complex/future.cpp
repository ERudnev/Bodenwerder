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
}
