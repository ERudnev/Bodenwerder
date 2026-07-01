#pragma once

#include <fQSM/features/rules/structural.h>

namespace fqsm::aspect::reaction::internal {


    using Behavior = ::fqsm::features::Behavior;

    template<typename Meta>
    struct Any  {

        inline static const Behavior defaultReactions = {};

        static const Behavior allReactions() {
            static const Behavior value = Behavior::merged(
                Meta::BaseReactions::defaultReactions,
                Meta::Reactions::custom
            );
            return value;
        }
    };

    template<typename Meta>
    struct Standalone : Any<Meta> {
        using Own = Any<Meta>;
    };

    template<typename Meta, typename HostType>
    struct Parasitic : Any<Meta> {
        using Own = Any<Meta>;
        using Parent = Standalone<HostType>;

        inline static const Behavior defaultReactions = Behavior::merged(
            Any<Meta>::defaultReactions,
            Behavior{
                features::reactions::rules::structural::remove_with_parent<Meta, HostType>(),
                features::reactions::rules::structural::parastic_requires_parent_to_appear<Meta, HostType>(),
            }
        );
    };
}

namespace fqsm::aspect::reaction {


    template<typename Meta>
    struct Entity : internal::Standalone<Meta> {
        using Behavior = internal::Behavior;
    };

    template<typename Meta, typename WorkerType>
    struct Controller : internal::Standalone<Meta> {
        using Behavior = internal::Behavior;
    };

    template<typename Meta, typename HostType>
    struct Attribute : internal::Parasitic<Meta, HostType> {
        using Behavior = internal::Behavior;
    };

    template<typename Meta, typename HostType>
    struct Component : internal::Parasitic<Meta, HostType> {
        using Behavior = internal::Behavior;

        inline static const internal::Behavior defaultReactions = internal::Behavior::merged(
            internal::Parasitic<Meta, HostType>::defaultReactions,
            internal::Behavior{
                features::reactions::rules::structural::dead_component_kill_parent<Meta, HostType>(),
                features::reactions::rules::structural::parrent_appears_requires_component<Meta, HostType>(),
            }
        );
    };
}
