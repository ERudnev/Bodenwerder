#pragma once

#include <base/containers_deprecated/delta.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/references.h>
#include <fQSM/state/slice/actual.h>
#include <fQSM/state/slice/patch.h>

namespace fqsm::state::slice {

    template<aspect::Any Meta>
    struct Delta {
        using Items = base::Delta<Id<Meta>, Quantum<Meta>>;
        using ItemChange = base::Change<Id<Meta>, Quantum<Meta>>;
        using StateSlice = Actual<Meta>;
        using PatchSlice = Patch<Meta>;
        using StateInterpretation = typename Items::StateInterpretation;

        const Items items;
        const cref<StateSlice> state;
        const cref<PatchSlice> patch;

        Delta(cref<StateSlice> state, cref<PatchSlice> patch, StateInterpretation interpretation)
            : items{state->items(), patch->elements, interpretation}
            , state{state}
            , patch{patch}
        {}
    };

}
