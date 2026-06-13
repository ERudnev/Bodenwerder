#pragma once

#include <base/containers/tableOverlay.h>

#include <fQSM/state/slice/view.h>

namespace fqsm::state::slice {
    namespace axis = meta::axis;

    template<aspect::Any Meta>
    struct Delta;

    template<aspect::Any Meta>
    struct Preview : View<Meta, axis::order::state> {
        using StateSlice = View<Meta, axis::order::state>;
        using PatchSlice = View<Meta, axis::order::patch>;
        using Item = typename StateSlice::Item;
        using Global = typename StateSlice::Global;
        using ItemsView = typename StateSlice::ItemsView;

        Preview(cref<StateSlice> state, cref<PatchSlice> patch) : state(state), patch(patch), table(state->items(), patch->items()) {}

        const ItemsView& items() const override { return table; }
        const Global& global() const override {
            if (patch->global().has_value()) {
                return patch->global().value();
            }
            return state->global();
        }

    private:
        template<aspect::Any>
        friend struct Delta;

        cref<StateSlice> state;
        cref<PatchSlice> patch;
        base::TableOverlay<Id<Meta>, Item> table;
    };
}
