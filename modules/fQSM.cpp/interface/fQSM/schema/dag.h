#pragma once

#include <string>
#include <unordered_map>

#include <fQSM/features/_forwards.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/schema/_forwards.h>
#include <fQSM/schema/binding.h>

namespace fqsm::schema {

    struct Dag {
        using TypeSet = meta::aspect::TypeSet;
        using Reactions = features::Normas;

        struct Node {
            std::string name;
            TypeSet origins;
            TypeSet followers;

            // TODO: consider to remove
            const features::Codex& codex;
            //const Reactions reactions;
            Binding binding;            
        };
    
        std::unordered_map<aspect::Rtid, Node, aspect::Rtid::Hash> nodes;
    };
}