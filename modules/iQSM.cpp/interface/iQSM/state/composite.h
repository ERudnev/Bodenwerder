#pragma once

#include <map>

#include <iQSM/meta/concepts.h>
#include <iQSM/references.h>
#include <iQSM/state/slice.h>
#include <iQSM/typeId.h>

namespace iqsm::state {

    template<policy::role Role, policy::versioning SlicePolicy>
    struct Composite {
        using TypeId = internals::Types::RuntimeId;
        using Layer = LayerFor<SlicePolicy>;

        template<meta::Aspect Meta>
        using Shared = slice::Data<Meta, Item<Meta, policy::versioning::shared, Role>>;

        template<meta::Aspect Meta>
        using Single = slice::Data<Meta, Item<Meta, policy::versioning::single, Role>>;

        Layer shared;
        Layer single;
    };
    
}