#pragma once

// KQM: &iQSM::Aspect

#include <type_traits>

#include <base/containers/denseTable.h>
#include <fQSM/meta/axis.h>
#include <fQSM/meta/mechanism/state.h>
#include <fQSM/meta/concepts.h>


// conveience alias to namespace with all aspect::Aby, aspect::Component, e.t.c.
namespace fqsm {
    namespace aspect = ::fqsm::meta::aspect;
}

namespace fqsm::meta {

    // TODO: tyr this as "alias host" and populate.. or remove and stay with "alias.h"-like alias  
    template<aspect::Any Meta> // it was more complicated in iQSM: template< ... , axis::versioning VersioningType>
    struct Aspect {
        struct Element {
            template<axis::order Order>
            using Item = typename meta::state::ItemsLayout<Meta, Order>::Element;

            using State = Item<axis::order::state>;
            using Patch = Item<axis::order::patch>; 
        };

        /* TODO: remove?
        struct Slice {
            template<axis::order Order>
            using Container = base::DenseTable<Id<Meta>, typename Element::template Item<Order>>;

            using State = iqsm::state::slice::Data<Meta, axis::order::state>;
            using Patch = iqsm::state::slice::Data<Meta, axis::order::patch>;
        };
        */
    };

}
