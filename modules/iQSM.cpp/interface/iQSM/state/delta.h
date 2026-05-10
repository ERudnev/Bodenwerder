#pragma once

#include <iQSM/state/layer.h>

namespace iqsm::state {
    
    // DeltaData is "patch" of "single-version" (mutable) containers if (mutable+immutable Slices)
    //struct DeltaData : Composite<policy::role::patch, policy::versioning::single> {
    //};

    struct DeltaData {
        using Container = SlicesLayout<policy::versioning::single>::SlicesContainer;
        //template<meta::Aspect Meta,         
        Container slices;

        bool empty() const;
    };
}