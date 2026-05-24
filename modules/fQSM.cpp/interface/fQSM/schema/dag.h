#pragma once

#include <set>
#include <unordered_map>

#include <fQSM/meta/runtimeId.h>
#include <fQSM/schema/_forwards.h>
#include <fQSM/schema/binding.h>

namespace fqsm::schema {

    namespace aspect = meta::aspect;

    struct Dag {
        using TypeSet = std::set<aspect::Rtid>;
        // NB: using Invariants = validation::Block;

        struct Node {
            aspect::Name name;
            TypeSet requiredByMe;
            TypeSet requiredBy;

            Binding binding;            
        };
    
        std::unordered_map<aspect::Rtid, Node, aspect::Rtid::Hash> nodes;
    };
}