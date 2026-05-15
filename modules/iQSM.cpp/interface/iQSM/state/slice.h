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

    template<axis::order Order, axis::versioning Versioning>
    struct Abstract {
        virtual ~Abstract() = default;
    };

    //template<axis::versioning Versioning>
    //using AstractState = VeryAbstract<axis::order::state, Versioning>;

    template<aspect::Any Meta, axis::order Order>
    struct Data : Abstract<Order, Meta::Runtime::Versioning::value> {
        using Chunk = typename Meta::Runtime::State::template Item<Order>;
        using Container = base::DenseTable<Id<Meta>, Chunk>;
        Container container;
    };
}
