#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <fQSM/features/_forwards.h>
#include <fQSM/identifier.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/schema/_forwards.h>
#include <fQSM/schema/binding.h>

namespace fqsm::schema {

    struct Dag {
        using ReactionId = Identifier<features::Reaction, std::size_t>;
        using ReactionIds = std::vector<ReactionId>;
        using Reactions = features::Reactions;

        struct Node {
            std::string name;
            ReactionIds reactions;
            Binding binding;
        };

        std::unordered_map<aspect::Rtid, Node, aspect::Rtid::Hash> nodes;
        Reactions reactions;
    };
}