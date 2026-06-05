#pragma once

#include <base/containers/denseTable.h>
#include <base/containers/tableOverlay.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/specializations.h>
#include <fQSM/state/_forwards.h>

// local usings and forwards
namespace fqsm::state::slice {
    namespace axis = meta::axis;

    template<aspect::Any Meta>
    struct Delta;
}

namespace fqsm::state::slice {    

    template<axis::order Order>
    struct Abstract {
        virtual ~Abstract() = default;
    };

    template<aspect::Any Meta, axis::order Order>
    struct View : Abstract<Order> {
        using Item = typename meta::state::DataLayout<Meta, Order>::Item;
        using Global = typename meta::state::DataLayout<Meta, Order>::Global;
        using ItemsView = base::TableView<Id<Meta>, Item>;

        virtual const ItemsView& items() const = 0;
        virtual const Global& global() const = 0;
    };

    template<aspect::Any Meta, axis::order Order>
    struct Data : View<Meta, Order> {
        using Item = typename View<Meta, Order>::Item;
        using Global = typename View<Meta, Order>::Global;
        using ItemsView = typename View<Meta, Order>::ItemsView;
        using ItemsData = base::DenseTable<Id<Meta>, Item>;

        const ItemsView& items() const override { return table; };
        const Global& global() const override { return globalValue; }

        ItemsData& items() { return table; }
        Global& global() { return globalValue; }

    private:
        ItemsData table;
        Global globalValue;
    };

    template<aspect::Any Meta>
    struct Overlay : View<Meta, axis::order::state> {
        using StateSlice = View<Meta, axis::order::state>;
        using PatchSlice = View<Meta, axis::order::patch>;
        using Item = typename StateSlice::Item;
        using Global = typename StateSlice::Global;
        using ItemsView = typename StateSlice::ItemsView;

        Overlay(cref<StateSlice> state, cref<PatchSlice> patch) : state(state), patch(patch), table(state->items(), patch->items()) {}

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
