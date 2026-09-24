#pragma once

#include <fQSM/processing/_forwards.h>
#include <fQSM/features/behavior.h>

// Structural rules of the categories (host and group lifecycle) are not reactions here:
// the schema derives them from each aspect's descriptor (see erased/rules.h).
namespace fqsm::aspect::internals {

    struct Base {
        using Reading = ::fqsm::Reading;
        using Writing = ::fqsm::Writing;
        using Reacting = ::fqsm::Reacting;
        using Retrospecting = ::fqsm::Retrospecting;
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
    struct Standalone : Any<Meta> {};

    template<typename Meta, typename HostType>
    struct Parasitic : Any<Meta> {};

    // Final categories
    template<typename Meta>
    struct Entity : Standalone<Meta> {
        inline static const Base::Behavior reactions() { return Meta::customAspectReactions(); }
    };

    template<typename Meta, typename HostType>
    struct Attribute : Parasitic<Meta, HostType> {
        inline static const Base::Behavior reactions() { return Meta::customAspectReactions(); }
    };

    template<typename Meta, typename HostType>
    struct Feature : Parasitic<Meta, HostType> {
        inline static const Base::Behavior reactions() { return Meta::customAspectReactions(); }
    };

    template<typename Meta, typename HostType>
    struct Component : Parasitic<Meta, HostType> {
        inline static const Base::Behavior reactions() { return Meta::customAspectReactions(); }
    };

    template<typename Meta, typename HostType, typename ElementType>
    struct Group : Parasitic<Meta, HostType> {
        inline static const Base::Behavior reactions() { return Meta::customAspectReactions(); }
    };
}
