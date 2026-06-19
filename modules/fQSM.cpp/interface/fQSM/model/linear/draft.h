#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {

    template<aspect::Any Meta>
    class Draft : public State<Meta> {
    public:
        Draft(const linear::State<Meta>& state, ref<linear::Patch<Meta>> patch) : itemsDraft(state, patch), patchGlobal(patch.global) {}

        virtual Items& items() override { return itemsDraft; }
        virtual const Items& items() override { return itemsDraft; }
        virtual Global& global() = 0;
        virtual const Global& global() const = 0;

    private:
        struct DraftGlobal {

        };

        ref<linear::Pa


        //base::cannonball::Draft<Id<Meta>, Quantum<Meta>> itemsDraft;
        //base::cannonball::Patchlet& patchGlobal;
    };
}