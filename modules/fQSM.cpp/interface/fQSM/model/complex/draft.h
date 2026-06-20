#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/complex/state.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/linear/delta.h>

namespace fqsm::model::complex {

    class Draft : public State {
    public:
        Draft(const State& state, ref<Patch> patch, const aspect::Rtid::Set& dirty = {})
            : State(state.schema), state(state), patch(patch), dirty(std::move(dirty)) { initStructure(); }

        template<aspect::Any Meta>
        linear::Delta<Meta> delta() const;

        cref<Patch> retreivePatch() const { return fqsm::freeze(patch); }

    private:
        void initStructure();
        // impl as State (entry builder)
        cref<Erased> getLine(meta::aspect::Rtid) const override;
        ref<Erased> getLine(meta::aspect::Rtid) override;
        const State::Composition& composition() const override { return lines; }
        State::Composition& composition() override { return lines; }

        const State& state; // yep, technically, Draft may be Draft over Draft which is over Draft. Be carefull!
        ref<Patch> patch;
        aspect::Rtid::Set dirty; // add the way to mark as dirty..
        Composite<linear::state::Erased> lines;
    };
}

namespace fqsm::model::complex {

    template<aspect::Any Meta>
    linear::Delta<Meta> Draft::delta() const {
        using Delta = linear::Delta<Meta>;
        const auto mode = dirty.contains(TypeId<Meta>) ? Delta::Mode::dirty : Delta::Mode::clean;
        return Delta{state.aspect<Meta>(), patch->aspect<Meta>(), mode};
    }
}
