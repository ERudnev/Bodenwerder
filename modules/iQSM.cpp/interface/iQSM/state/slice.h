#pragma once

#include <base/containers/denseTable.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/alias.h>
#include <iQSM/state/_forwards.h>
#include <iQSM/state/mechanism.h>

namespace iqsm::state::slice {

    template<policy::order Order, policy::versioning Versioning>
    struct VeryAbstract {
        virtual ~VeryAbstract() = default;
    };

    template<policy::versioning Versioning>
    using AstractState = VeryAbstract<policy::order::state, Versioning>;

    template<meta::Aspect Meta, policy::order Order, policy::versioning Versioning>
    struct Data : VeryAbstract<Order, Versioning> {
        using Chunk = ::iqsm::state::Chunk<Meta, Versioning, Order>;
        using Container = base::DenseTable<Id<Meta>, Chunk>;
        Container container;
    };
}
