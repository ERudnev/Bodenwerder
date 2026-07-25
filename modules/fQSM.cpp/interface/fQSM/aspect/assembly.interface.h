#pragma once

#include <unordered_set>

#include <fQSM/identifier.h>
//#include <fQSM/features/interface.include.h>
//#include <fQSM/features/reactions/categories.h>
#include <fQSM/aspect/actions.h>
#include <fQSM/aspect/internals.h>

namespace fqsm::detail::aspect {

    struct Base {
        // Aspect internal alias:
        template<typename Meta>
        using Anchor = Identifier<Meta>;

        template<typename Meta>
        using Custody = Identifier<Meta>;

        template<typename Meta>
        using Affected = ::fqsm::Affected<Meta>;
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

    // DefaultInternals = reactions base + home BaseActions (no with<Home> in Internals).
    // Disambiguate shared typedefs only; capability methods come from ActionsBase whole.
    template<typename InternalsBase, typename ActionsBase>
    struct InternalsWithActions : InternalsBase, ActionsBase {
        using Id = typename ActionsBase::Id;
        using Quantum = typename ActionsBase::Quantum;
        using Reading = typename InternalsBase::Reading;
        using Writing = typename InternalsBase::Writing;
        using Reacting = typename InternalsBase::Reacting;
        using Retrospecting = typename InternalsBase::Retrospecting;
        using Behavior = typename InternalsBase::Behavior;
        using PossibleChange = typename InternalsBase::PossibleChange;
    };

}

// TODO: make attempt to use concepts at meta/categories.h
// this looks obvious but can make CRTP very "C" for compiler

namespace fqsm::aspect {

    template<typename Meta>
    struct Entity : detail::aspect::Standalone<Meta> {
        using BaseActions = actions::Entity<Meta>;
        using DefaultInternals = detail::aspect::InternalsWithActions<
            internals::Entity<Meta>, BaseActions>;
    };

    template<typename Meta, typename HostType>
    struct Attribute : detail::aspect::Parasitic<Meta, HostType> {
        using HostAspect = HostType;
        using BaseActions = actions::Attribute<Meta, HostType>;
        using DefaultInternals = detail::aspect::InternalsWithActions<
            internals::Attribute<Meta, HostType>, BaseActions>;
    };

    template<typename Meta, typename HostType>
    struct Feature : detail::aspect::Parasitic<Meta, HostType> {
        using HostAspect = HostType;
        using BaseActions = actions::Feature<Meta, HostType>;
        using DefaultInternals = detail::aspect::InternalsWithActions<
            internals::Feature<Meta, HostType>, BaseActions>;
    };

    template<typename Meta, typename HostType>
    struct Component : detail::aspect::Parasitic<Meta, HostType> {
        using HostAspect = HostType;
        using BaseActions = actions::Component<Meta, HostType>;
        using DefaultInternals = detail::aspect::InternalsWithActions<
            internals::Component<Meta, HostType>, BaseActions>;
    };

    template<typename Meta, typename HostType, typename WorkerType>
    struct Group : detail::aspect::Parasitic<Meta, HostType> {
        using Quantum = std::unordered_set<typename WorkerType::Id>;
        using HostAspect = HostType;
        using BaseActions = actions::Group<Meta, HostType, WorkerType>;
        using DefaultInternals = detail::aspect::InternalsWithActions<
            internals::Group<Meta, HostType, WorkerType>, BaseActions>;
        using WorkerAspect = WorkerType;
    };

    template<typename Meta>
    struct Archetype : actions::Archetype {
        using BaseActions = Meta;
    };

    template<typename Meta, meta::category::Any PrimaryType>
    struct Manipulation : actions::Manipulation {
        using PrimaryAspect = PrimaryType;
        using Id = typename PrimaryType::Id;
        using Quantum = typename PrimaryType::Quantum;
        using BaseActions = Meta;
    };
}
