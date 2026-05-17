#pragma once

#include <unordered_map>
#include <set>
#include <string>

#include <fQSM/meta/state.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/state/slice.h>
#include <fQSM/references.h>
//#include <fQSM/service/validation.h>

namespace fqsm::state {

    namespace axis = meta::axis;
    
    struct SchemaData {
        using TypeSet = std::set<RAId>;
        //using Invariants = validation::Block;

        struct Aspect {
            const std::string name; // persistent name
            TypeSet requiredByMe;
            TypeSet requiredBy;
        };

        std::unordered_map<RAId, Aspect> aspects;
    };
}