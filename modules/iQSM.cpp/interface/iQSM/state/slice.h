#pragma once

#include <base/containers/denseTable.h>
#include <iQSM/meta/axis.h>
#include <iQSM/meta/mechanism/state.h>
#include <iQSM/meta/concepts/aspect.h>
#include <iQSM/meta/alias.h>
#include <iQSM/state/_forwards.h>

namespace iqsm::state::slice {

    namespace axis = meta::axis;
    namespace aspect = meta::aspect;

    template<axis::order Order>
    struct Abstract {
        virtual ~Abstract() = default;
    };

    template<axis::order Order, axis::versioning Versioning>
    struct AbstractVersioned : Abstract<Order> {
        virtual ~AbstractVersioned() = default;
    };

    template<aspect::Any Meta, axis::order Order>
    struct Data : AbstractVersioned<Order, Meta::Runtime::Versioning::value> {
        using Item = typename Meta::Runtime::State::template Item<Order>;
        using Container = base::DenseTable<Id<Meta>, Item>;
        Container container;
    };
}
