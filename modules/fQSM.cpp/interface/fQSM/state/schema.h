#pragma once

#include <unordered_map>
#include <set>
#include <string>

#include <fQSM/meta/runtimeId.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/state/slice.h>
#include <fQSM/references.h>
//#include <fQSM/service/validation.h>

namespace fqsm::state {

    namespace axis = meta::axis;
    
    struct SchemaData {
        using TypeSet = std::set<meta::aspect::Rtid>;
        //using Invariants = validation::Block;

        struct Aspect {
            const meta::aspect::Name name;
            TypeSet requiredByMe;
            TypeSet requiredBy;
        };

        std::unordered_map<meta::aspect::Rtid, Aspect, meta::aspect::Rtid::Hash> aspects;
    };
}