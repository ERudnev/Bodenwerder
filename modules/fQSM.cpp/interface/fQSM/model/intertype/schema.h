#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <fQSM/erased/descriptor.h>
#include <fQSM/features/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/identifier.h>
#include <fQSM/model/intertype/set.h>

namespace fqsm::model::intertype {

    struct Graph {
        using ReactionId = Identifier<features::reactions::Abstract, std::size_t>;
        using ReactionIds = std::vector<ReactionId>;
        using Reactions = features::Reactions;
        using Slot = std::uint32_t;

        struct Node {
            std::string name;
            ReactionIds reactions;
            Slot slot;
        };

        template<category::Any Meta>
        bool accepts() const { return nodes.contains(TypeId<Meta>); }

        // Dense slot of an aspect; throws std::out_of_range for an aspect outside this schema.
        Slot slotOf(Rtid id) const { return nodes.at(id).slot; }
        std::size_t slotCount() const { return descriptors.size(); }

        std::unordered_map<Rtid, Node, Rtid::Hash> nodes;
        std::vector<erased::Descriptor> descriptors;   // by slot, registration order
        Reactions reactions;
    };
}
