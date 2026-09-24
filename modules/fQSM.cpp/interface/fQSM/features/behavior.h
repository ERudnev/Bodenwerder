#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <fQSM/features/_forwards.h>
#include <fQSM/features/reaction.h>

namespace fqsm::features {

    // The reactions of one aspect, as returned by customAspectReactions().
    struct Behavior {
        const Reactions rules;

        Behavior() = default;
        Behavior(Reactions in) : rules(std::move(in)) {}

        // syntax sugar for aspect definitions: return { reactionA(), reactionB(p) };
        template<typename... Rs>
            requires (sizeof...(Rs) > 0 and (std::is_base_of_v<reactions::Abstract, std::decay_t<Rs>> and ...))
        Behavior(Rs&&... reactions) : rules{std::make_shared<std::decay_t<Rs>>(std::forward<Rs>(reactions))...} {}
    };
}
