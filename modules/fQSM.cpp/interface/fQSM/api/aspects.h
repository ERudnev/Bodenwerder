#pragma once

#include <fQSM/identifier.h>
#include <fQSM/features/interface.include.h>
#include <fQSM/features/reactions/categories.h>

namespace fqsm::detail::aspects {
    template<typename Meta>
    struct Any {
        Any() = delete;
        using Behavior = fqsm::features::Behavior;

        static const Behavior allReactions() {
            return reactions::Any<Meta>::allReactions();
        }
    };
}

namespace fqsm::aspects {

    // this classes are base of final Aspects (metaclasses)
    template<typename Meta>
    struct Standalone : detail::aspects::Any<Meta> {
        using Id = Identifier<Meta>;
    };

    template<typename Meta, typename HostType>
    struct Parasitic : detail::aspects::Any<Meta> {
        using Id = typename HostType::Id;
        using HostAspect = HostType;
    };

    template<typename Meta>
    struct Entity : Standalone<Meta> {
        using BaseActions = actions::Entity<Meta>;
        using BaseReactions = reactions::Entity<Meta>;
    };

    template<typename Meta, typename WorkerType>
    struct Controller : Standalone<Meta> {
        using BaseActions = actions::Controller<Meta, WorkerType>;
        using BaseReactions = reactions::Controller<Meta, WorkerType>;
        using WorkerAspect = WorkerType;
    };

    template<typename Meta, typename HostType>
    struct Attribute : Parasitic<Meta, HostType> {
        using BaseActions = actions::Attribute<Meta, HostType>;
        using BaseReactions = reactions::Attribute<Meta, HostType>;
    };

    template<typename Meta, typename HostType>
    struct Component : Parasitic<Meta, HostType> {
        using BaseActions = actions::Component<Meta, HostType>;
        using BaseReactions = reactions::Component<Meta, HostType>;
    };

    // Interpretation of several types:
    struct Archetype : actions::Archetype {
        using BaseReactions = reactions::Archetype;
        //using BaseActions = actions::Archetype;
    };
}
