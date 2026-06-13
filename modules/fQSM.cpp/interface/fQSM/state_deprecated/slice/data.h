#pragma once

#include <base/containers/denseTable_deprecated.h>

#include <fQSM/state/slice/view.h>

namespace fqsm::state::slice {

    template<aspect::Any Meta, axis::order Order>
    struct Data : View<Meta, Order> {
        using Item = typename View<Meta, Order>::Item;
        using Global = typename View<Meta, Order>::Global;
        using ItemsView = typename View<Meta, Order>::ItemsView;
        using ItemsData = base::DenseTable_deprecated<Id<Meta>, Item>;

        const ItemsView& items() const override { return table; };
        const Global& global() const override { return globalValue; }

        ItemsData& items() { return table; }
        Global& global() { return globalValue; }

    private:
        ItemsData table;
        Global globalValue;
    };

}
