#pragma once

#include <set>
#include <string>
#include <unordered_map>

#include <fQSM/features/_forwards.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/schema/_forwards.h>
#include <fQSM/schema/binding.h>

namespace fqsm::schema {

    struct Dag {
        using TypeSet = std::set<aspect::Rtid>;
        // NB: using Normas = validation::Block;

        struct Node {
            std::string name;
            TypeSet origins;
            TypeSet followers;

            const features::Codex& codex;
            Binding binding;            
        };
    
        std::unordered_map<aspect::Rtid, Node, aspect::Rtid::Hash> nodes;
    };
}