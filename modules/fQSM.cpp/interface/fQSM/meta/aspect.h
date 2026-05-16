#pragma once

// KQM: &iQSM::Aspect

#include <type_traits>

#include <base/containers/denseTable.h>
#include <fQSM/meta/axis.h>
#include <fQSM/meta/mechanism/state.h>
#include <fQSM/meta/concepts/aspects.h>
#include <fQSM/meta/concepts/archetypes.h>


// conveience alias to namespace with all aspect::Aby, aspect::Component, e.t.c.
namespace fqsm {
    namespace aspect = ::fqsm::meta::aspect;
}

namespace fqsm::meta {

    template<archetype::Any Meta> // it was more complicated in iQSM: template< ... , axis::versioning VersioningType>
    struct Aspect : Meta {
        struct Runtime {
            struct Element {
                template<axis::order Order>
                using Item = typename meta::state::ItemsLayout<Aspect, Order>::Element;

                using State = Item<axis::order::state>;
                using Patch = Item<axis::order::patch>; 
            };

            struct Slice {
                template<axis::order Order>
                using Container = base::DenseTable<Id<Meta>, typename Element::template Item<Order>>;

                using State = iqsm::state::slice::Data<Aspect, axis::order::state>;
                using Patch = iqsm::state::slice::Data<Aspect, axis::order::patch>;
            };
        };
    };
}
