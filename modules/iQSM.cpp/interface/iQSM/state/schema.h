#pragma once

#include <unordered_map>
#include <set>
#include <string>

#include <iQSM/state/_forwards.h>
#include <iQSM/references.h>
#include <iQSM/service/validation.h>

namespace iqsm::state {
    
    struct SchemaData {
        using TypeSet = std::set<RAId>;
        using Invariants = validation::Block;

        template<axis::versioning Versioning>
        struct Aspect {
            std::string name; // persistent name
            axis::versioning layer; // TODO: remove (it is template parameter now)
            cref<slice::AbstractState<Versioning>> zero;
            TypeSet requiredByMe;
            TypeSet requiredBy;
        };

        using VersionedAspect = Aspect<axis::versioning::shared>;
        using OperationalAspect = Aspect<axis::versioning::single>;

        std::unordered_map<RAId, VersionedAspect> versioned;
        std::unordered_map<RAId, OperationalAspect> operational;

        /* TODO: use this fields to populate updated Aspect struct
        struct Aspect {
            std::string name; // persistent name
            axis::versioning layer;
            cref<slice::Abstract> zero;
            TypeSet requiredByMe;
            TypeSet requiredBy;
            Versioned versioned;
            Operational operational;
        };
        */
    };
}