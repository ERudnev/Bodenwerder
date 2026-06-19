#pragma once

#include <initializer_list>

#include <base/shared_reference.h>

#include <fQSM/features/codex.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/model/structure/details/builders.h>

namespace fqsm::manipulation::schema {

    Schema merge(std::initializer_list<Schema> parts);

    template<meta::aspect::Any Meta>
    Schema aspect();
}
// impl:
namespace fqsm::manipulation::schema {
    inline Schema merge(std::initializer_list<Schema> parts) {
        auto out = base::make_shared<fqsm::schema::Dag>();

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
                const auto reactionId = fqsm::schema::Dag::ReactionId{ out->reactions.size() };
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

    template<meta::aspect::Any Meta>
    fqsm::Schema aspect() {
        auto out = base::make_shared<fqsm::schema::Dag>();
        auto node = fqsm::schema::Dag::Node{
            std::string{fqsm::meta::aspect::Rtid::name<Meta>()},
            fqsm::schema::Dag::ReactionIds{},
            fqsm::schema::details::binding<Meta>(),
        };

        out->nodes.emplace(fqsm::meta::aspect::Rtid::of<Meta>(), node);

        for (const auto& reaction : Meta::codex.morms) {
            const auto reactionId = fqsm::schema::Dag::ReactionId{ out->reactions.size() };
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