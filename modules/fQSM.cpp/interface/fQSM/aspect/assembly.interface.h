#pragma once

#include <unordered_set>

#include <fQSM/identifier.h>
//#include <fQSM/features/interface.include.h>
//#include <fQSM/features/reactions/categories.h>
#include <fQSM/aspect/actions.h>
#include <fQSM/aspect/internals.h>

// workshop:
#include <type_traits>

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

    template<typename Meta>
    struct Standalone : detail::aspect::Any<Meta> {
        using Id = Identifier<Meta>;
    };

    template<typename Meta, typename HostType>
    struct Parasitic : detail::aspect::Any<Meta> {
        using Id = typename HostType::Id;
    };

}

// TODO: make attempt to use concepts at meta/categories.h
// this looks obvious but can make CRTP very "C" for compiler

namespace fqsm::aspect {

    template<typename Meta>
    struct Entity : detail::aspect::Standalone<Meta> {
        using BaseActions = actions::Entity<Meta>;
        using DefaultInternals = internals::Entity<Meta>;
    };

    template<typename Meta, typename HostType>
    struct Attribute : detail::aspect::Parasitic<Meta, HostType> {
        using HostAspect = HostType;
        using BaseActions = actions::Attribute<Meta, HostType>;
        using DefaultInternals = internals::Attribute<Meta, HostType>;
    };


    // temp:
    template<typename Meta, typename HostType>
    struct Component : detail::aspect::Parasitic<Meta, HostType> {
    };

    template<typename Meta, typename HostType, typename ElementType>
    struct Group : detail::aspect::Parasitic<Meta, HostType> {
    };

    template<typename Meta>
    struct Archetype : actions::Archetype {
        //using BaseActions = Meta;
    };




    /*
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
        using BaseActions = actions::Entity<Meta>;
        using BaseReactions = reactions::Entity<Meta>;
        using DefaultBehavior = reactions::Default<Meta>;
    };

    template<typename Meta, typename HostType>
    struct Attribute : Parasitic<Meta, HostType> {
        using BaseActions = actions::Attribute<Meta, HostType>;
        using BaseReactions = reactions::Attribute<Meta, HostType>;
        using DefaultBehavior = reactions::Default<Meta>;
    };

    template<typename Meta, typename HostType>
    struct Component : Parasitic<Meta, HostType> {
        using BaseActions = actions::Component<Meta, HostType>;
        using BaseReactions = reactions::Component<Meta, HostType>;
        using DefaultReactions = reactions::Default<Meta>;
        using CustomReactions = reactions::Custom<Meta>;
    };

    // Thin parasitic group of managed Elements
    template<typename Meta, typename HostType, typename ElementType>
    struct Group : Parasitic<Meta, HostType> {
        using Quantum = std::unordered_set<typename ElementType::Id>;
        using BaseActions = actions::Group<Meta, HostType, ElementType>;
        using BaseReactions = reactions::Group<Meta, HostType, ElementType>;
        struct Actions final : BaseActions {};
        using Reactions = reactions::Default<Meta>;
        using ElementAspect = ElementType;
        using WorkerAspect = ElementType;
    };

    // Interpretation helper: archetype resolves actions to the final Meta itself.
    template<typename Meta>
    struct Archetype : actions::Archetype {
        using BaseActions = Meta;
    };*/
}
