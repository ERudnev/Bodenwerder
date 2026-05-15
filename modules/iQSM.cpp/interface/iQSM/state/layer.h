#pragma once

// terms:
// "Layer" is set of Slices with oen kind of (versioning+order)

#include <map>

#include <iQSM/meta/axis.h>
#include <iQSM/meta/aspect.h>
#include <iQSM/references.h>
#include <iQSM/state/_forwards.h>
#include <iQSM/state/slice.h>

namespace iqsm::state {

    // TODO: do something with too long "meta::axis:..."
    template<meta::axis::order ItemRole, meta::axis::versioning ItemVersioning, meta::axis::versioning SliceVersioning>
    struct Layer  {
        //using SlicesContainer = SlicesLayout<SliceVersioning>::SlicesContainer;
        
        SlicesContainer slices;
    };    
}