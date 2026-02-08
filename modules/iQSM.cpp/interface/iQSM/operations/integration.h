#pragma once

#include <iQSM/world.h>
#include <iQSM/delta.h>

namespace iqsm {
    World integrate(World, Delta);
    World validate(World);
    Delta merge(Delta first, Delta second); // first/second is order of appearance
}


