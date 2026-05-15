#pragma once

#include <iQSM/state/layer.h>

namespace iqsm::state {
    
    // DeltaData is "patch" of "single-version" (mutable) containers if (mutable+immutable Slices)
    //struct DeltaData : Composite<axis::order::patch, axis::versioning::single> {
    //};

    struct DeltaData {
        SlicesLayout<axis::versioning::shared>::SlicesContainer versioned;
        SlicesLayout<axis::versioning::single>::SlicesContainer operational;

        bool empty() const;
    };
}