#pragma once

#include <iQSM/world.h>
#include <iQSM/delta.h>

namespace iqsm::ops {

    // Reliable default: always validates the resulting world.
    World integrate(World, Delta); // TODO: consider rename to "apply"??

    // Fast path: applies delta without validation (for Transaction and internal pipelines).
    World integrate_raw(World, Delta);
    World validate(World);
    Delta merge(Delta first, Delta second); // first/second is order of appearance
}


