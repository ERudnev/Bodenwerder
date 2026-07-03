#pragma once

#include <unordered_set>

#include <fQSM/identifier.h>
//#include <fQSM/features/interface.include.h>
//#include <fQSM/features/reactions/categories.h>
#include <fQSM/aspect/action.h>
#include <fQSM/aspect/reaction.h>

namespace fqsm::detail::aspect {

    struct Base {
        // Aspect internal alias:
        template<typename Meta>
        using Anchor = Identifier<Meta>;

        template<typename Meta>
        using Control = Identifier<Meta>;
    };

    template<typename Meta>
    struct Any : Base {
        Any() = delete;
    };
}

// TODO: make attempt to use concepts at meta/categories.h
// this looks obvious but can make CRTP very "C" for compiler

namespace fqsm::aspect {

    // this classes are base of final Aspects (metaclasses)
    template<typename Meta>
    struct Standalone : detail::aspect::Any<Meta> {
        using Id = Identifier<Meta>;
    };

    template<typename Meta, typename HostType>
    struct Parasitic : detail::aspect::Any<Meta> {
        using Id = typename HostType::Id;
        using HostAspect = HostType;
    };

    // Final categories
    template<typename Meta>
    struct Entity : Standalone<Meta> {
        using BaseActions = action::Entity<Meta>;
        using BaseReactions = reaction::Entity<Meta>;
        struct DefaultActions final : BaseActions {};
        struct DefaultReactions final : BaseReactions { inline static const features::Behavior custom{}; };
    };

    template<typename Meta, typename HostType>
    struct Attribute : Parasitic<Meta, HostType> {
        using BaseActions = action::Attribute<Meta, HostType>;
        using BaseReactions = reaction::Attribute<Meta, HostType>;
        struct DefaultActions final : BaseActions {};
        struct DefaultReactions final : BaseReactions { inline static const features::Behavior custom{}; };
    };

    template<typename Meta, typename HostType>
    struct Component : Parasitic<Meta, HostType> {
        using BaseActions = action::Component<Meta, HostType>;
        using BaseReactions = reaction::Component<Meta, HostType>;
        struct DefaultActions final : BaseActions {};
        struct DefaultReactions final : BaseReactions { inline static const features::Behavior custom{}; };
    };

    // Thin parasitic group of managed Elements
    template<typename Meta, typename HostType, typename ElementType>
    struct Group : Parasitic<Meta, HostType> {
        using Quantum = std::unordered_set<typename ElementType::Id>;
        using BaseActions = action::Group<Meta, HostType, ElementType>;
        using BaseReactions = reaction::Group<Meta, HostType, ElementType>;
        struct Actions final : BaseActions {};
        struct Reactions final : BaseReactions { inline static const features::Behavior custom{}; };
        using ElementAspect = ElementType;
        using WorkerAspect = ElementType;
    };

    // Interpretation helper: archetype resolves actions to the final Meta itself.
    template<typename Meta>
    struct Archetype : action::Archetype {
        using BaseActions = Meta;
    };
}
