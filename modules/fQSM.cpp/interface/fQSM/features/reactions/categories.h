#pragma once

#include <fQSM/features/rules/structural.h>

namespace fqsm::reactions {

    struct Base {
        using Behavior = ::fqsm::features::Behavior;

        inline static const Behavior defaultReactions = {};
    };

    template<typename Meta>
    struct Any : Base {
        inline static const Behavior defaultReactions = Behavior::merged(
            Base::defaultReactions,
            Behavior{}
        );

        static const Behavior& allReactions() {
            static const Behavior value = Behavior::merged(Meta::BaseReactions::defaultReactions, Meta::custom);
            return value;
        }
    };

    template<typename Meta>
    struct Standalone : Any<Meta> {
        using Own = Any<Meta>;

        inline static const Base::Behavior defaultReactions = Base::Behavior::merged(
            Any<Meta>::defaultReactions,
            Base::Behavior{}
        );
    };

    template<typename Meta, typename HostType>
    struct Parasitic : Any<Meta> {
        using Own = Any<Meta>;
        using Parent = Standalone<HostType>;

        inline static const Base::Behavior defaultReactions = Base::Behavior::merged(
            Any<Meta>::defaultReactions,
            Base::Behavior{
                features::reactions::rules::structural::remove_with_parent<Meta, HostType>(),
                features::reactions::rules::structural::parastic_requires_parent_to_appear<Meta, HostType>(),
            }
        );
    };

    template<typename Meta>
    using Entity = Standalone<Meta>;

    template<typename Meta, typename WorkerType>
    using Controller = Standalone<Meta>;

    template<typename Meta, typename HostType>
    using Attribute = Parasitic<Meta, HostType>;

    template<typename Meta, typename HostType>
    struct Component : Parasitic<Meta, HostType> {
        inline static const Base::Behavior defaultReactions = Base::Behavior::merged(
            Parasitic<Meta, HostType>::defaultReactions,
            Base::Behavior{
                features::reactions::rules::structural::dead_component_kill_parent<Meta, HostType>(),
                features::reactions::rules::structural::parrent_appears_requires_component<Meta, HostType>(),
            }
        );
    };

    struct Archetype : Base {
        inline static const Behavior defaultReactions = Behavior::merged(
            Base::defaultReactions,
            Behavior{}
        );
    };
}
