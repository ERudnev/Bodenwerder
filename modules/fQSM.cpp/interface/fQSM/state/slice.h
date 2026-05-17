#pragma once

#include <base/containers/denseTable.h>
#include <fQSM/meta/axis.h>
#include <fQSM/meta/state.h>
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
    struct Data : Abstract<Order> {
        using Item = typename Meta::Runtime::Element::template Item<Order>;
        using Container = base::DenseTable<Id<Meta>, Item>;
        Container container;
    };
}
