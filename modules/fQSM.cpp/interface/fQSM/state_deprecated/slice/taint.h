#pragma once

#include <fQSM/state/slice/view.h>

namespace fqsm::state::slice {

    template<aspect::Any Meta>
    struct Taint : View<Meta, axis::order::patch> {
        using ItemsView = typename View<Meta, axis::order::patch>::ItemsView;
        using Global = typename View<Meta, axis::order::patch>::Global;
        using ItemsData = base::DenseTable_deprecated<Id<Meta>, typename View<Meta, axis::order::patch>::Item>;

        const ItemsView& items() const override { return table; }
        const Global& global() const override { return globalValue; }
        bool tainted() const override { return true; }

    private:
        ItemsData table;
        Global globalValue;
    };
}
