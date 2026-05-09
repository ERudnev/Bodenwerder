#pragma once

// terms:
// "Layer" is set of Slices with oen kind of (versioning+role)

#include <map>

#include <iQSM/meta/concepts.h>
#include <iQSM/references.h>
#include <iQSM/state/_forwards.h>
#include <iQSM/state/slice.h>

namespace iqsm::state {

    template<policy::role ItemRole, policy::versioning ItemVersioning, policy::versioning SliceVersioning>
    struct Layer  {
        using SlicesContainer = SlicesLayout<SliceVersioning>::SlicesContainer;
        SlicesContainer slices;
        //using SliceContainer = slice::Data<Meta, Versioning, ItemRole>;
    };

    /*
    template<policy::role Role, policy::versioning SlicePolicy>
    struct Composite {
        using Layer = LayerFor<SlicePolicy>;

        // remove?
        template<meta::Aspect Meta>
        using Shared = slice::Data<Meta, Item<Meta, policy::versioning::shared, Role>>;

        template<meta::Aspect Meta>
        using Single = slice::Data<Meta, Item<Meta, policy::versioning::single, Role>>;
        //
        const Schema schema;
        Layer shared;
        Layer single;
    };*/
    
}