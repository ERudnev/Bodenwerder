#pragma once

#include <fQSM/references.h>

// model forwards
namespace fqsm::model::complex {
    struct StateOperable;
    struct StateAddressable;
    struct Patch;
}

// alias, mostly for external use
namespace fqsm::model {
    using World = complex::StateOperable;
    using WorldRaw = complex::StateAddressable;
    using Patch = complex::Patch;
}
