#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <fQSM/features/_forwards.h>
#include <fQSM/features/reaction.h>

namespace fqsm::features {

    struct Behavior {
        const Reactions rules;

        Behavior() = default;

        Behavior(Reactions in) : rules(std::move(in)) {}

        // syntax sugar for Aspect definitions: {normaA(), normaB(p), normaC(c,d)}
        template<typename FirstReaction, typename... RestReactions, typename = std::enable_if_t<std::is_base_of_v<features::reactions::Abstract, std::decay_t<FirstReaction>> && (std::is_base_of_v<features::reactions::Abstract, std::decay_t<RestReactions>> && ...)>>
        Behavior(FirstReaction&& firstReaction, RestReactions&&... restReactions)
            : rules(make_normas(std::forward<FirstReaction>(firstReaction), std::forward<RestReactions>(restReactions)...)) {}

    private:
        template<typename FirstReaction, typename... RestReactions>
        static Reactions make_normas(FirstReaction&& firstReaction, RestReactions&&... restReactions) {
            Reactions out;
            out.reserve(1 + sizeof...(RestReactions));

            out.push_back(std::make_shared<std::decay_t<FirstReaction>>(std::forward<FirstReaction>(firstReaction)));
            (out.push_back(std::make_shared<std::decay_t<RestReactions>>(std::forward<RestReactions>(restReactions))), ...);

            return out;
        }
    };
}