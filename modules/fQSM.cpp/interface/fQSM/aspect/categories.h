#pragma once

#include <fQSM/identifier.h>
//#include <fQSM/features/interface.include.h>
//#include <fQSM/features/reactions/categories.h>
#include <fQSM/aspect/action.h>
#include <fQSM/aspect/reaction.h>

namespace fqsm::detail::aspect {
    template<typename Meta>
    struct Any {
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
    };

    template<typename Meta, typename WorkerType>
    struct Controller : Standalone<Meta> {
        using BaseActions = action::Controller<Meta, WorkerType>;
        using BaseReactions = reaction::Controller<Meta, WorkerType>;
        using WorkerAspect = WorkerType;
    };

    template<typename Meta, typename HostType>
    struct Attribute : Parasitic<Meta, HostType> {
        using BaseActions = action::Attribute<Meta, HostType>;
        using BaseReactions = reaction::Attribute<Meta, HostType>;
    };

    template<typename Meta, typename HostType>
    struct Component : Parasitic<Meta, HostType> {
        using BaseActions = action::Component<Meta, HostType>;
        using BaseReactions = reaction::Component<Meta, HostType>;
    };

    // Interpretation of several types:
    struct Archetype : action::Archetype {
    };
}
