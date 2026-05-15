#pragma once

// KQM: &iQSM::Aspect

#include <type_traits>

#include <iQSM/meta/concepts/aspect.h>
#include <iQSM/meta/concepts/archetype.h>
#include <iQSM/meta/mechanism/state.h>
#include <iQSM/state/slice.h>

// conveience alias to namespace with all aspect::Aby, aspect::Component, e.t.c.
namespace iqsm {
    namespace aspect = ::iqsm::meta::aspect;
}

namespace iqsm::meta {

    template<archetype::Any Meta, axis::versioning VersioningType>
    struct Aspect : Meta {
        struct Runtime {
            using Versioning = std::integral_constant<axis::versioning, VersioningType>;
            struct State {
                template<axis::order Order>
                using Item = typename meta::state::ItemsLayout<Aspect, VersioningType, Order>::Element;
            };

            struct Slice {
                //using LayerLayout =
            };
        };
    };
}
