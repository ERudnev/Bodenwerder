#pragma once

// terms:
// "Layer" is set of Slices with oen kind of (versioning+order)

#include <map>

#include <iQSM/meta/concepts.h>
#include <iQSM/references.h>
#include <iQSM/state/_forwards.h>
#include <iQSM/state/slice.h>

namespace iqsm::state {

    template<policy::order ItemRole, policy::versioning ItemVersioning, policy::versioning SliceVersioning>
    struct Layer  {
        using SlicesContainer = SlicesLayout<SliceVersioning>::SlicesContainer;
        SlicesContainer slices;
    };    
}