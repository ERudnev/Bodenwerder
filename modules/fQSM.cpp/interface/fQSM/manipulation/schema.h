#pragma once

#include <initializer_list>

#include <base/shared_reference.h>

#include <fQSM/features/behavior.h>
#include <fQSM/meta/categories.h>
#include <fQSM/model/structure/schema.h>
#include <fQSM/model/structure/builders.h>

namespace fqsm::manipulation::schema {

    Schema merge(std::initializer_list<Schema> parts);

    template<meta::category::Any Meta>
    Schema aspect();
}
// impl:
namespace fqsm::manipulation::schema {
    inline Schema merge(std::initializer_list<Schema> parts) {
        auto out = base::make_shared<model::structure::AspectGraph>();

        for (const auto& part : parts) {
            for (const auto& [type, node] : part->nodes) {
                out->nodes.emplace(type, node);
            }
        }

        for (auto& [_, node] : out->nodes) {
            node.reactions.clear();
        }

        for (const auto& part : parts) {
            for (const auto& reaction : part->reactions) {
                const auto reactionId = model::structure::AspectGraph::ReactionId{ out->reactions.size() };
                out->reactions.push_back(reaction);

                for (const auto& sourceType : reaction->listens()) {
                    const auto found = out->nodes.find(sourceType);
                    if (found == out->nodes.end()) continue;
                    found->second.reactions.push_back(reactionId);
                }
            }
        }

        return fqsm::freeze(out);
    }

    template<meta::category::Any Meta>
    fqsm::Schema aspect() {
        auto out = base::make_shared<model::structure::AspectGraph>();
        auto node = model::structure::AspectGraph::Node{
            std::string{fqsm::meta::Rtid::name<Meta>()},
            model::structure::AspectGraph::ReactionIds{},
            fqsm::schema::details::binding<Meta>(),
        };

        out->nodes.emplace(TypeId<Meta>, node);

        for (const auto& reaction : Meta::allReactions().rules) {
            const auto reactionId = model::structure::AspectGraph::ReactionId{ out->reactions.size() };
            out->reactions.push_back(reaction);

            for (const auto& sourceType : reaction->listens()) {
                const auto found = out->nodes.find(sourceType);
                if (found == out->nodes.end()) continue;
                found->second.reactions.push_back(reactionId);
            }
        }

        return fqsm::freeze(out);
    }
}