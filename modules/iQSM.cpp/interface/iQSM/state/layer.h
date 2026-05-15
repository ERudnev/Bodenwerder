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
    namespace axis = meta::axis;

    template<axis::order ItemRole, axis::versioning ItemVersioning, axis::versioning SliceVersioning>
    struct Layer {
        using SliceBase = slice::AbstractVersioned<ItemRole, ItemVersioning>;
        using Layout = meta::state::SlicesLayout<SliceVersioning>;
        using SlicesContainer = Layout::template Container<SliceBase>;

        SlicesContainer slices;
    };
}