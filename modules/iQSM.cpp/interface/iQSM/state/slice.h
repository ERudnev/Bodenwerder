#pragma once

#include <base/containers/denseTable.h>
#include <iQSM/meta/concepts/aspect.h>
#include <iQSM/meta/alias.h>
#include <iQSM/state/_forwards.h>
#include <iQSM/state/mechanism.h>

namespace iqsm::state::slice {

    template<policy::order Order, policy::versioning Versioning>
    struct ErasedAspect {
        virtual ~ErasedAspect() = default;
    };

    //template<policy::versioning Versioning>
    //using AstractState = VeryAbstract<policy::order::state, Versioning>;

    template<meta::aspect::Any Meta, policy::order Order, policy::versioning Versioning>
    struct Abstract : VeryAbstract<Order, Versioning> {
        //using Chunk = ::iqsm::state::Chunk<Meta, Versioning, Order>; 
        using Container = base::DenseTable<Id<Meta>, Chunk>;
        Container container;
    };
}
