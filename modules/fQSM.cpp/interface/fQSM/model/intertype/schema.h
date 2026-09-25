#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <fQSM/erased/descriptor.h>
#include <fQSM/erased/links.h>
#include <fQSM/erased/rules.h>
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

        // Rebuilds rules from the descriptors; rules whose aspects are not all in the schema are skipped.
        void deriveRules();
        // Rebuilds links from the reactions; links whose aspects are not all in the schema are skipped.
        void deriveLinks();
        // Index into links, or npos when no reaction declared that link.
        static constexpr std::size_t npos = static_cast<std::size_t>(-1);
        std::size_t linkOf(Slot client, Slot observed, erased::LinkReader read) const;

        std::unordered_map<Rtid, Node, Rtid::Hash> nodes;
        std::vector<erased::Descriptor> descriptors;   // by slot, registration order
        std::vector<erased::Rule> rules;                // structural rules of the categories
        std::vector<erased::Link> links;                // links the reactions declared, one inbound index each
        std::vector<std::vector<std::size_t>> linksOfClient;   // by slot: indexes into links whose client is the slot
        Reactions reactions;
    };
}
