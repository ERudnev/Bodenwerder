#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <fQSM/features/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/identifier.h>
#include <fQSM/model/intertype/binding.h>
#include <fQSM/model/intertype/set.h>

namespace fqsm::model::intertype {

    struct Graph {
        using ReactionId = Identifier<features::reactions::Abstract, std::size_t>;
        using ReactionIds = std::vector<ReactionId>;
        using Reactions = features::Reactions;

        struct Node {
            std::string name;
            ReactionIds reactions;
            Binding binding;
            // Migration marker: true when aspect was registered for archive.
            // Future: replaced/filled by a type-erased archive slot (cf. Binding).
            bool persistent = false;
        };

        // sugar:
        template<category::Any Meta>
        bool accepts() const { return nodes.contains(TypeId<Meta>); }

        std::unordered_map<Rtid, Node, Rtid::Hash> nodes;
        Reactions reactions;
    };
}