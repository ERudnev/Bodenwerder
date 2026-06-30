#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/model/complex/state.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/linear/future.h>
#include <fQSM/model/linear/delta.h>

namespace fqsm::model::complex {

    class Future : public State {
    public:
        Future(const State& state, ref<Patch> patch, const Rtid::Set& dirty = {})
            : State(state.schema), state(state), changes(patch), dirty(std::move(dirty)) { initStructure(); }

        template<category::Any Meta>
        linear::Delta<Meta> delta() const;

        ref<Patch> patch() { return changes; }
        cref<Patch> patch() const { return fqsm::freeze(changes); }

    private:
        void initStructure();
        // impl as State (entry builder)
        cref<Erased> getLine(meta::Rtid typeId) const override { return lines.container.at(typeId); }
        ref<Erased> getLine(meta::Rtid typeId) override { return lines.container.at(typeId); }
        const State::Composite& composition() const override { return lines; }
        State::Composite& composition() override { return lines; }

        const State& state; // yep, technically, Future may be Future over Future which is over Future. Be carefull!
        ref<Patch> changes;
        Rtid::Set dirty; // add the way to mark as dirty..
        intertype::Composite<linear::state::Erased> lines;
    };
}

namespace fqsm::model::complex {

    template<category::Any Meta>
    linear::Delta<Meta> Future::delta() const {
        using Delta = linear::Delta<Meta>;
        const auto mode = dirty.contains(TypeId<Meta>) ? Delta::Mode::dirty : Delta::Mode::clean;
        return Delta{state.aspect<Meta>(), changes->aspect<Meta>(), mode};
    }

    inline void Future::initStructure() {
        for (const auto& [typeId, node] : schema->nodes) {
            lines.container.emplace(typeId, node.binding.createFuture(state, changes));
        }
    }
}
