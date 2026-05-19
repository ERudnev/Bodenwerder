#pragma once

#include <base/containers/denseTable.h>
#include <fQSM/meta/axis.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/meta/alias.h>
#include <fQSM/state/_forwards.h>

namespace fqsm::state::slice {

    namespace axis = meta::axis;
    namespace aspect = meta::aspect;

    template<axis::order Order>
    struct Abstract {
        virtual ~Abstract() = default;
    };

    template<aspect::Any Meta, axis::order Order>
    struct View : Abstract<Order> {
        using Item = typename Meta::Runtime::Element::template Item<Order>;
        using ItemsView = base::TableView<Id<Meta>, Item>;

        virtual const ItemsView& items() const = 0;
    };

    template<aspect::Any Meta, axis::order Order>
    struct Data : View<Meta, Order> {
        using Item = typename View<Meta, Order>::Item;
        using ItemsView = typename View<Meta, Order>::ItemsView;
        using ItemsData = base::DenseTable<Id<Meta>, Item>;

        const ItemsView& items() const override {
            return table;
        };

        ItemsData& items() {
            return table;
        }

    private:
        ItemsData table;
    };
}

