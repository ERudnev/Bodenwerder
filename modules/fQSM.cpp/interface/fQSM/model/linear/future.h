#pragma once

#include <base/cannonball/future.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    class Future : public State<Meta> {
    public:
        using Mode = base::cannonball::SeeChanges;
        using Items = State<Meta>::Items;
        using Global = State<Meta>::Global;

        Future(const linear::State<Meta>& state, ref<linear::Patch<Meta>> patch, Mode mode)
            : draftItems(state.items(), patch->items, mode), futureGlobal(state.global(), patch->global) {}

        virtual Items& items() override { return draftItems; }
        virtual const Items& items() const override { return draftItems; }
        virtual Global& global() override { return futureGlobal.access(); }
        virtual const Global& global() const override { return futureGlobal.get(); }

    private:
        struct FutureGlobal {
            const Global& stateGlobal;
            base::cannonball::Patchlet<Global>& patchGlobal;
            Mode mode;
            const Global& get() const {
                if (mode == Mode::blind) return stateGlobal;
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