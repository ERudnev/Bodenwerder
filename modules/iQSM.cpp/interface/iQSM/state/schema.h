#pragma once

#include <map>
#include <set>
#include <string>

#include <iQSM/typeId.h>
#include <iQSM/references.h>
#include <iQSM/service/validation.h>

namespace iqsm::state {
    
    struct SchemaData {
        using TypeId = internals::Types::RuntimeId;
        using TypeSet = std::set<TypeId>;
        using Invariants = validation::Block;

        struct Aspect {
            struct Versioned {
                // TODO: add this stuff: cref<slice::Abstract> zero;
            };
            struct Operational {
                
            };

            std::string name; // persistent name
            TypeSet requiredByMe;
            TypeSet requiredBy;
            Versioned versioned;
            Operational operational;
        };

        std::map<TypeId, Aspect> aspects;
    };
}