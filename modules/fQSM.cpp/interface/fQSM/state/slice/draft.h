#pragma once

#include <base/containers_deprecated/overlay.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/references.h>
#include <fQSM/state/slice/actual.h>
#include <fQSM/state/slice/interface.h>
#include <fQSM/state/slice/patch.h>

namespace fqsm::state::slice {

    template<aspect::Any Meta>
    struct Draft : Write<Meta> {
        using StateSlice = Actual<Meta>;
        using PatchSlice = Patch<Meta>;
        using Item = Quantum<Meta>;
        using Container = typename Write<Meta>::Container;
        using Global = typename Write<Meta>::Global;

        Draft(cref<StateSlice> state, ref<PatchSlice> patch)
            : state(state)
            , patch(patch)
            , table(state->items(), patch->elements)
        {}

        Container& items() override { return table; }

        Global& global() override {
            if (patch->global.has_value()) {
                return patch->global.value();
            }
            return state->global();
        }

    private:
        cref<StateSlice> state;
        ref<PatchSlice> patch;
        base::Overlay<Id<Meta>, Item> table;
    };

}
