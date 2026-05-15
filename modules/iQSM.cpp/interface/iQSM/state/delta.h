#pragma once

#include <iQSM/state/layer.h>

namespace iqsm::state {
    
    // DeltaData is "patch" of "single-version" (mutable) containers if (mutable+immutable Slices)
    //struct DeltaData : Composite<axis::order::patch, axis::versioning::single> {
    //};

    struct DeltaData {
        using SliceBase = slice::Abstract<axis::order::patch>;
        using SlicesContainer = std::unordered_map<RAId, ref<SliceBase>>;
    
        SlicesContainer slices;
    
        bool empty() const { return slices.empty(); }
    };
}