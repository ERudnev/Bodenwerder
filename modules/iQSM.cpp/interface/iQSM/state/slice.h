#pragma once

#include <base/containers/denseTable.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/alias.h>
#include <iQSM/state/mechanism.h>

namespace iqsm::state::slice {

    struct Abstract {
        virtual ~Abstract() = default;
    };

    template<meta::Aspect Meta, typename Item>
    struct Data : Abstract {
        using Id = ::iqsm::Id<Meta>;
        using Container = base::DenseTable<Id, Item>;

        Container container;
    };
}
