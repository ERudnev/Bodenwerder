#pragma once

#include <initializer_list>
#include <string>

#include <base/shared_reference.h>

#include <fQSM/erased/descriptor.h>
#include <fQSM/features/behavior.h>
#include <fQSM/meta/categories.h>
#include <fQSM/model/intertype/schema.h>

namespace fqsm::manipulation::schema {

    Schema merge(std::initializer_list<Schema> parts);

    template<meta::category::Any Meta>
    Schema aspect();
}
// impl:
namespace fqsm::manipulation::schema {

    namespace detail {
        using Graph = model::intertype::Graph;

        inline void add_reaction(Graph& graph, const Graph::Reactions::value_type& reaction) {
            const auto reactionId = Graph::ReactionId{ graph.reactions.size() };
            graph.reactions.push_back(reaction);
            for (const auto& sourceType : reaction->listens()) {
                const auto found = graph.nodes.find(sourceType);
                if (found == graph.nodes.end()) continue;
                found->second.reactions.push_back(reactionId);
            }
        }

        inline void add_node(Graph& graph, const erased::Descriptor& descriptor) {
            if (graph.nodes.contains(descriptor.id)) return;
            const auto slot = static_cast<Graph::Slot>(graph.descriptors.size());
            graph.descriptors.push_back(descriptor);
            graph.nodes.emplace(descriptor.id, Graph::Node{std::string{descriptor.name}, {}, slot});
        }
    }

    // Slots are renumbered: parts in order, each part in its own slot order, first registration wins.
    // The reactions are those of the kept descriptors: an aspect registered twice brings its reactions once.
    inline Schema merge(std::initializer_list<Schema> parts) {
        auto out = base::make_shared<model::intertype::Graph>();

        for (const auto& part : parts)
            for (const auto& descriptor : part->descriptors)
                detail::add_node(*out, descriptor);

        for (const auto& descriptor : out->descriptors)
            for (const auto& reaction : descriptor.reactions)
                detail::add_reaction(*out, reaction);

        out->deriveRules();
        out->deriveLinks();
        return fqsm::freeze(out);
    }

    template<meta::category::Any Meta>
    fqsm::Schema aspect() {
        auto out = base::make_shared<model::intertype::Graph>();
        detail::add_node(*out, erased::describe<Meta>());
        for (const auto& reaction : out->descriptors.front().reactions)
            detail::add_reaction(*out, reaction);
        out->deriveRules();
        out->deriveLinks();
        return fqsm::freeze(out);
    }
}
