#pragma once

#include <fQSM/identifier.h>
#include <fQSM/features/interface.include.h>

namespace fqsm::detail::aspects {
    template<typename Meta>
    struct Any {
        Any() = delete;
        using Behavior = fqsm::features::Behavior;
    };
}

namespace fqsm::aspects {

    // this classes are base of final Aspects (metaclasses)
    template<typename Meta>
    struct Entity : detail::aspects::Any<Meta> {
        using Id = Identifier<Meta>;
        using BaseActions = actions::Entity<Meta>;
    };

    template<typename Meta, typename WorkerType>
    struct Controller : detail::aspects::Any<Meta> {
        using Id = Identifier<Meta>;
        using BaseActions = actions::Controller<Meta, WorkerType>;
        using WorkerAspect = WorkerType;
    };

    template<typename Meta, typename HostType>
    struct Attribute : detail::aspects::Any<Meta> {
        using Id = typename HostType::Id;
        using HostAspect = HostType;
        using BaseActions = actions::Attribute<Meta, HostType>;
    };

    template<typename Meta, typename HostType>
    struct Component : detail::aspects::Any<Meta> {
        using Id = typename HostType::Id;
        using HostAspect = HostType;
        using BaseActions = actions::Component<Meta, HostType>;
    };

    // Interpretation of several types:
    struct Archetype : actions::Archetype {
        //using BaseActions = actions::Archetype;
    };
}
