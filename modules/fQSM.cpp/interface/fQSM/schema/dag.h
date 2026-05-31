#pragma once

#include <set>
#include <string>
#include <unordered_map>

#include <fQSM/meta/runtimeId.h>
#include <fQSM/schema/_forwards.h>
#include <fQSM/schema/binding.h>

namespace fqsm::schema {

    struct Dag {
        using TypeSet = std::set<aspect::Rtid>;
        // NB: using Invariants = validation::Block;

        struct Node {
            std::string name;
            TypeSet requiredByMe;
            TypeSet requiredBy;

            Binding binding;            
        };
    
        std::unordered_map<aspect::Rtid, Node, aspect::Rtid::Hash> nodes;
    };
}