#pragma once

#include <map>
#include <set>
#include <string>

#include <iQSM/state/_forwards.h>
#include <iQSM/references.h>
#include <iQSM/service/validation.h>

namespace iqsm::state {
    
    struct SchemaData {
        using TypeSet = std::set<RAId>;
        using Invariants = validation::Block;

        struct Aspect {
            struct Versioned {
            };
            struct Operational {
            };

            std::string name; // persistent name
            policy::versioning layer;
            cref<slice::Abstract> zero;
            TypeSet requiredByMe;
            TypeSet requiredBy;
            Versioned versioned;
            Operational operational;
        };

        std::map<RAId, Aspect> aspects;
    };
}