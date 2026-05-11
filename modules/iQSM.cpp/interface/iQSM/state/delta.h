#pragma once

#include <iQSM/state/layer.h>

namespace iqsm::state {
    
    // DeltaData is "patch" of "single-version" (mutable) containers if (mutable+immutable Slices)
    //struct DeltaData : Composite<policy::order::patch, policy::versioning::single> {
    //};

    struct DeltaData {
        SlicesLayout<policy::versioning::shared>::SlicesContainer versioned;
        SlicesLayout<policy::versioning::single>::SlicesContainer operational;

        bool empty() const;
    };
}