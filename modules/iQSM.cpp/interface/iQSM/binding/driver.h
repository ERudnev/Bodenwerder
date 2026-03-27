#pragma once

#include <iQSM/meta/concepts.h>

// details
namespace iqsm::detail::binding {
    // make domain' internal types of "Handlers" accesable
    template<meta::Binding Meta>
    struct driver_of;

    template<meta::Binding Meta>
    using driver_t = typename driver_of<Meta>::type;

    struct Driver {
    };
}