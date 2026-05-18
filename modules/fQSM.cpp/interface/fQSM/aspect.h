#pragma once

// Library-facing aspect facade.
// Sits above low-level meta/state layers and gathers the aliases that upper library layers actually consume.

#include <type_traits>

#include <base/containers/denseTable.h>
#include <fQSM/meta/axis.h>
#include <fQSM/meta/specializations.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/state/slice.h>

// conveience alias to namespace with all aspect::Aby, aspect::Component, e.t.c.
namespace fqsm {
    namespace aspect = ::fqsm::meta::aspect;
}

namespace fqsm {
    // TODO: tyr this as "alias host" and populate.. or remove and stay with "alias.h"-like alias  
    template<aspect::Any Meta> // it was more complicated in iQSM: template< ... , axis::versioning VersioningType>
    struct Aspect {
        struct Element {
            template<meta::axis::order Order>
            using Item = typename meta::state::ItemsLayout<Meta, Order>::Element;

            using State = Item<meta::axis::order::state>;
            using Patch = Item<meta::axis::order::patch>; 
        };

        struct Slice {
            using State = fqsm::state::slice::Data<Meta, meta::axis::order::state>;
            using Patch = fqsm::state::slice::Data<Meta, meta::axis::order::patch>;
        };
    };

}
