#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <fQSM/features/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/identifier.h>
#include <fQSM/model/structure/binding.h>

namespace fqsm::model::structure {

    struct AspectGraph {
        using ReactionId = Identifier<features::Reaction, std::size_t>;
        using ReactionIds = std::vector<ReactionId>;
        using Reactions = features::Reactions;

        struct Node {
            std::string name;
            ReactionIds reactions;
            Binding binding;
        };

        // sugar:
        template<category::Any Meta>
        bool accepts() const { return nodes.contains(TypeId<Meta>); }

        std::unordered_map<Rtid, Node, Rtid::Hash> nodes;
        Reactions reactions;
    };
}