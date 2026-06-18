#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <fQSM/features/_forwards.h>
#include <fQSM/identifier.h>
#include <fQSM/meta/rtid.h>
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
        template<aspect::Any Meta>
        bool accepts() const { return nodes.contains(aspect::Rtid::of<Meta>()); }

        std::unordered_map<aspect::Rtid, Node, aspect::Rtid::Hash> nodes;
        Reactions reactions;
    };
}