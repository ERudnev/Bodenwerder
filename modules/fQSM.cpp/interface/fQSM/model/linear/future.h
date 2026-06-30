#pragma once

#include <base/cannonball/future.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    class Future : public State<Meta> {
    public:
        using Items = State<Meta>::Items;
        using Global = State<Meta>::Global;

        Future(const linear::State<Meta>& state, ref<linear::Patch<Meta>> patch)
            : draftItems(state.items(), patch->items, base::cannonball::SeeChanges::observable)
            , futureGlobal(state.global(), patch->global)
        {}

        virtual Items& items() override { return draftItems; }
        virtual const Items& items() const override { return draftItems; }
        virtual Global& global() override { return futureGlobal.access(); }
        virtual const Global& global() const override { return futureGlobal.get(); }

    private:
        struct FutureGlobal {
            const Global& stateGlobal;
            base::cannonball::Patchlet<Global>& patchGlobal;
            const Global& get() const {
                if (patchGlobal) return patchGlobal.value();
                return stateGlobal;
            }
            Global& access() {
                if (not patchGlobal)
                    patchGlobal = stateGlobal;
                return patchGlobal.value();
            }
        };

        base::cannonball::Future<Id<Meta>, Quantum<Meta>> draftItems;
        FutureGlobal futureGlobal;
    };
}