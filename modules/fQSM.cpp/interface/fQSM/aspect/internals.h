#pragma once

#include <fQSM/processing/_forwards.h>
#include <fQSM/features/behavior.h>
#include <fQSM/features/reactions/structural.h>

namespace fqsm::aspect::internals {

    struct Base {
        using Reading = ::fqsm::Reading;
        using Writing = ::fqsm::Writing;
        using Reacting = ::fqsm::Reacting;
        using Behavior = ::fqsm::features::Behavior;
    };

    template<typename Meta>
    struct Any : Base {
        using Id = typename Meta::Id;
        using Quantum = typename Meta::Quantum;

        // TODO: consider an option to put into: struct Vocabulary {
            using JustReacting = std::function<void(Writing, Id, const Quantum&)>;
            using PossibleChange = std::optional<Quantum>;
            using TemporaryFreeReaction = std::function<void(Reacting)>;
        // [end]TODO};

        inline static const Behavior reactions() {
            return Behavior{};
        }
    };

    template<typename Meta>
    struct Standalone : Any<Meta> {
        using Behavior = Base::Behavior;
        inline static const Behavior reactions() {
            return Behavior::merged(
                Any<Meta>::reactions(),
                Behavior{}
            );
        }
    };

    template<typename Meta, typename HostType>
    struct Parasitic : Any<Meta> {
        using Behavior = Base::Behavior;
        inline static const Behavior reactions() {
            return Behavior::merged(
                Any<Meta>::reactions(),
                Behavior{
                    features::reactions::structural::remove_with_parent<Meta, HostType>(),
                }
            );
        };
    };

    // Final categories
    template<typename Meta>
    struct Entity : Standalone<Meta> {
        using Behavior = Base::Behavior;
        inline static const Behavior reactions() {
            return Behavior::merged(
                Behavior::merged(
                    Standalone<Meta>::reactions(),
                    Behavior{}
                ),
                Meta::customAspectReactions()
            );
        };
    };

    template<typename Meta, typename HostType>
    struct Attribute : Parasitic<Meta, HostType> {
        using Behavior = Base::Behavior;
        inline static const Behavior reactions() {
            return Behavior::merged(
                Behavior::merged(
                    Parasitic<Meta, HostType>::reactions(),
                    Behavior{
                        features::reactions::structural::new_parasitic_requires_existing_parent<Meta, HostType>(),
                    }
                ),
                Meta::customAspectReactions()
            );
        };
    };

    template<typename Meta, typename HostType>
    struct Feature : Parasitic<Meta, HostType> {
        using Behavior = Base::Behavior;
        inline static const Behavior reactions() {
            return Behavior::merged(
                Behavior::merged(
                    Parasitic<Meta, HostType>::reactions(),
                    Behavior{
                        features::reactions::structural::dead_parasitic_kill_parent<Meta, HostType>(),
                        features::reactions::structural::new_parasitic_requires_parent_appears<Meta, HostType>(),
                    }
                ),
                Meta::customAspectReactions()
            );
        };
    };

    template<typename Meta, typename HostType>
    struct Component : Parasitic<Meta, HostType> {
        using Behavior = Base::Behavior;
        inline static const Behavior reactions() {
            return Behavior::merged(
                Behavior::merged(
                    Parasitic<Meta, HostType>::reactions(),
                    Behavior{
                        features::reactions::structural::dead_parasitic_kill_parent<Meta, HostType>(),
                        features::reactions::structural::parent_appears_requires_component<Meta, HostType>(),
                        features::reactions::structural::new_parasitic_requires_parent_appears<Meta, HostType>(),
                    }
                ),
                Meta::customAspectReactions()
            );
        };
    };

    template<typename Meta, typename HostType, typename ElementType>
    struct Group : Parasitic<Meta, HostType> {
        using Behavior = Base::Behavior;
        inline static const Behavior reactions() {
            return Behavior::merged(
                Behavior::merged(
                    Parasitic<Meta, HostType>::reactions(),
                    Behavior{
                        features::reactions::structural::group_removal_removes_elements<Meta, ElementType>(),
                        features::reactions::structural::new_parasitic_requires_existing_parent<Meta, HostType>(),
                    }
                ),
                Meta::customAspectReactions()
            );
        };
    };
}