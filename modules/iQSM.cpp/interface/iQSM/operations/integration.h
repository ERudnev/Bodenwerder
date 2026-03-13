#pragma once

#include <iQSM/world.h>
#include <iQSM/delta.h>

namespace iqsm::ops {
    // Fast path: applies delta without validation (for Transaction and internal pipelines).
    World integrate(World, Delta);
    World validate(World);
    Delta merge(Delta first, Delta second); // first/second is order of appearance
}


