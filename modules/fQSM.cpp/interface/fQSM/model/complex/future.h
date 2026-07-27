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
        //using WorkersInterface::schema;
        //using WorkersInterface::stateLines;

        Future(const State& state, ref<Patch> patch, const Rtid::Set& dirty = {});

        template<category::Any Meta>
        linear::Delta<Meta> delta() const;

        // TODO: make it private to hide from workers.
        // idea: make Future derived from ObservableFuture if entity, created it requires patch() access
        ref<Patch> patch() { return changes; }
        cref<Patch> patch() const { return fqsm::freeze(changes); }

        Patch::Summary& summary() { return changes->summary; }
        const Patch::Summary& summary() const { return changes->summary; }

        // former parts of complex::WorkersInterface
        template<category::Any Meta>
        linear::WorkersInterface<Meta>& updates();

        template<category::Any Meta>
        const linear::WorkersInterface<Meta>& updates() const;

    private:
        void initStructure();
        // impl as State (entry builder)
        cref<Erased> getLine(meta::Rtid typeId) const override { return futureLines.container.at(typeId); }
        ref<Erased> getLine(meta::Rtid typeId) override { return futureLines.container.at(typeId); }
        const State::Composite& composition() const override { return futureLines; }
        State::Composite& composition() override { return futureLines; }

        const State& state; // yep, technically, Future may be Future over Future which is over Future. Be carefull!
        ref<Patch> changes;
        Rtid::Set dirty; // add the way to mark as dirty..
        intertype::Composite<linear::state::Erased> futureLines;
    };

    // TODO: make new interface "FutureObservable" and replace this "WorkersInterface"->"Future"
    using WorkersInterface = Future;
}

namespace fqsm::model::complex {

    inline Future::Future(const State& state, ref<Patch> patch, const Rtid::Set& dirty)
            : State(state.schema)
            , state(state)
            , changes(std::move(patch))
            , dirty(std::move(dirty))
    { initStructure(); }

    template<category::Any Meta>
    linear::WorkersInterface<Meta>& Future::updates() {
        //return static_cast<linear::Patch<Meta>&>(*patchLines.container.at(TypeId<Meta>).get());
        return static_cast<linear::Future<Meta>&>(*futureLines.container.at(TypeId<Meta>).get());
    }

    template<category::Any Meta>
    const linear::WorkersInterface<Meta>& Future::updates() const {
        //return static_cast<const linear::Patch<Meta>&>(*patchLines.container.at(TypeId<Meta>).get());
        return static_cast<linear::Future<Meta>&>(*futureLines.container.at(TypeId<Meta>).get());
    }

    template<category::Any Meta>
    linear::Delta<Meta> Future::delta() const {
        using Delta = linear::Delta<Meta>;
        const auto mode = dirty.contains(TypeId<Meta>) ? Delta::Mode::dirty : Delta::Mode::clean;
        return Delta{state.aspect<Meta>(), changes->aspect<Meta>(), mode};
    }

    inline void Future::initStructure() {
        for (const auto& [typeId, node] : schema->nodes) {
            futureLines.container.emplace(typeId, node.binding.createFuture(state, changes));
        }
    }

    /*
    template<category::Any Meta>
    linear::WorkersInterface<Meta>& WorkersInterface::updates() {
        return static_cast<linear::Patch<Meta>&>(*stateLines.container.at(TypeId<Meta>).get());
    };

    template<category::Any Meta>
    const linear::WorkersInterface<Meta>& WorkersInterface::updates() const {
        return static_cast<const linear::Patch<Meta>&>(*stateLines.container.at(TypeId<Meta>).get());
    }*/
}
