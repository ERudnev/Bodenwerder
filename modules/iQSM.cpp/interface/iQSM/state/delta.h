#pragma once

#include <iQSM/state/composite.h>

namespace iqsm::state {
    
    // DeltaData is "patch" of "single-version" (mutable) containers if (mutable+immutable Slices)
    struct DeltaData : Composite<policy::role::patch, policy::versioning::single> {
    };
}