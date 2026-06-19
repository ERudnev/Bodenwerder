#pragma once

#include <fQSM/meta/rtid.h>
#include <fQSM/model/complex/state.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/linear/delta.h>

namespace fqsm::model::complex {

    class Draft : public State {
        Draft(const State& state, Patch& patch, const aspect::Rtid::Set& dirty = {}) : State(state.schema), state(state), patch(patch), dirty(std::move(dirty)) {}

        template<aspect::Any Meta>
        linear::Delta<Meta> delta() const;

    private:
        // impl as State (entry builder)
        cref<Erased> aspect(meta::aspect::Rtid) const override;
        ref<Erased> aspect(meta::aspect::Rtid) override;

        const State& state; // yep, technically, Draft may be Draft over Draft which is over Draft. Consider to
        Patch& patch;
        const aspect::Rtid::Set dirty;
    };
}

namespace fqsm::model::complex {

    template<aspect::Any Meta>
    linear::Delta<Meta> Draft::delta() const {
        using Delta = linear::Delta<Meta>;
        const auto mode = dirty.contains(aspect::Rtid::of<Meta>()) ? Delta::Mode::dirty : Delta::Mode::clean;
        return Delta{state.aspect<Meta>(), patch.aspect<Meta>(), mode};
    }

}
