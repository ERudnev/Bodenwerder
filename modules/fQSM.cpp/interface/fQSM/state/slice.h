#pragma once

#include <base/containers/denseTable.h>
#include <base/containers/tableOverlay.h>
#include <fQSM/meta/axis.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/meta/alias.h>
#include <fQSM/state/_forwards.h>

// local usings and forwards
namespace fqsm::state::slice {
    namespace axis = meta::axis;
    namespace aspect = meta::aspect;

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
        using Item = typename meta::state::ItemsLayout<Meta, Order>::Element;
        using ItemsView = base::TableView<Id<Meta>, Item>;

        virtual const ItemsView& items() const = 0;
    };

    template<aspect::Any Meta, axis::order Order>
    struct Data : View<Meta, Order> {
        using Item = typename View<Meta, Order>::Item;
        using ItemsView = typename View<Meta, Order>::ItemsView;
        using ItemsData = base::DenseTable<Id<Meta>, Item>;

        const ItemsView& items() const override { return table; };

        ItemsData& items() { return table; }

    private:
        ItemsData table;
    };

    template<aspect::Any Meta>
    struct Overlay : View<Meta, axis::order::state> {
        using StateSlice = View<Meta, axis::order::state>;
        using PatchSlice = View<Meta, axis::order::patch>;
        using Item = typename StateSlice::Item;
        using ItemsView = typename StateSlice::ItemsView;

        Overlay(cref<StateSlice> state, cref<PatchSlice> patch) : state(state), patch(patch), table(state->items(), patch->items()) {}

        const ItemsView& items() const override { return table; }

    private:
        template<aspect::Any>
        friend struct Delta;

        cref<StateSlice> state;
        cref<PatchSlice> patch;
        base::TableOverlay<Id<Meta>, Item> table;
    };
}
