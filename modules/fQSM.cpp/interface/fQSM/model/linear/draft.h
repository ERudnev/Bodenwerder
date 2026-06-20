#pragma once

#include <base/cannonball/draft.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {

    template<aspect::Any Meta>
    class Draft : public State<Meta> {
    public:
        using Items = State<Meta>::Items;
        using Global = State<Meta>::Global;

        Draft(const linear::State<Meta>& state, ref<linear::Patch<Meta>> patch)
            : draftItems(state.items(), patch->items), draftGlobal(state.global(), patch->global) {}

        virtual Items& items() override { return draftItems; }
        virtual const Items& items() const override { return draftItems; }
        virtual Global& global() override { return draftGlobal.access(); }
        virtual const Global& global() const override { return draftGlobal.get(); }

    private:
        struct DraftGlobal {
            const Global& stateGlobal;
            base::cannonball::Patchlet<Global>& patchGlobal;
            const Global& get() const {
                if (patchGlobal)
                    return patchGlobal.value();
                return stateGlobal;
            }
            Global& access() {
                if (not patchGlobal)
                    patchGlobal = stateGlobal;
                return patchGlobal.value();
            }
        };

        base::cannonball::Draft<Id<Meta>, Quantum<Meta>> draftItems;
        DraftGlobal draftGlobal;
    };
}