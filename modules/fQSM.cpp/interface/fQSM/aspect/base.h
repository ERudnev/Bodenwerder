#pragma once

// Aspect bases: Entity, Attribute, Feature, Component, Group are one template (Base) over category data (Traits).
// Capability<Meta> is the facade behind with<Meta> and BaseActions: one class for every category;
// an operation that belongs to some categories only is constrained on Meta::Traits::category.

#include <functional>
#include <optional>
#include <type_traits>

#include <fQSM/id_set.h>
#include <fQSM/identifier.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/orchestrators/quantal.h>
#include <fQSM/features/behavior.h>
#include <fQSM/utility/messages.h>

namespace fqsm::aspect {

    template<typename Meta> struct Capability;
    template<typename Meta> struct Internals;

    namespace detail {
        template<typename Host> struct HostId { using type = typename Host::Id; };

        template<typename T> struct GroupQuantum {};
        template<typename Host, typename Element>
        struct GroupQuantum<Traits<Category::group, Host, Element>> { using Quantum = IdSet<typename Element::Id>; };

        // element placeholder of non-group aspects: keeps the group operations declarable
        struct NoElement { struct Id {}; struct Quantum {}; };

        // namespace scope on purpose: members of Capability would be found by lookup inside every Actions
        template<typename Meta> inline constexpr bool entity = Meta::Traits::category == Category::entity;
        template<typename Meta> inline constexpr bool group = Meta::Traits::category == Category::group;
        template<typename Meta> using HostOf = typename Meta::Traits::HostAspect;
        template<typename Meta> using ElementOf = std::conditional_t<group<Meta>, typename Meta::Traits::ElementAspect, NoElement>;
    }

    // Context names for facades that live inside an aspect.
    struct Contexts {
        using Reading = ::fqsm::Reading;
        using Writing = ::fqsm::Writing;
        using Stewarding = ::fqsm::Stewarding;
        template<meta::category::Any Meta> using Direct = ::fqsm::Direct<Meta>;
    };

    template<typename Meta, typename T>
    struct Base : detail::GroupQuantum<T> {
        Base() = delete;

        using Traits = T;
        using HostAspect = typename T::HostAspect;
        using Id = typename std::conditional_t<T::category == Category::entity,
            std::type_identity<Identifier<Meta>>, detail::HostId<HostAspect>>::type;
        using BaseActions = Capability<Meta>;
        using DefaultInternals = Internals<Meta>;

        // Q1 link fields: custody<T> and anchor<T> are ids with a lifecycle rule, affects<T> is a typed id only.
        template<typename W> using Anchor = typename W::Id;
        template<typename W> using Custody = typename W::Id;
        template<typename W> using Affected = ::fqsm::Affected<W>;
    };

    template<typename Meta> using Entity = Base<Meta, Traits<Category::entity>>;
    template<typename Meta, typename Host> using Attribute = Base<Meta, Traits<Category::attribute, Host>>;
    template<typename Meta, typename Host> using Feature = Base<Meta, Traits<Category::feature, Host>>;
    template<typename Meta, typename Host> using Component = Base<Meta, Traits<Category::component, Host>>;
    template<typename Meta, typename Host, typename Element> using Group = Base<Meta, Traits<Category::group, Host, Element>>;

    template<typename Meta>
    struct Capability : Contexts {
        using Id = typename Meta::Id;
        using Quantum = typename Meta::Quantum;
        using PossibleChange = std::optional<Quantum>;

        struct Vocabulary {
            using EvaluateQuantumLocal = std::function<PossibleChange(const Quantum&)>;
            using EvaluateQuantumContextual = std::function<PossibleChange(Reading, Id, const Quantum&)>;
            using JustRetrospecting = std::function<void(Retrospecting, Id, const Quantum&)>;
        };

        // every category
        static auto count(Reading context) -> std::size_t { return context->aspect<Meta>().items().size(); }
        static auto find(Reading context, Id id) -> const Quantum* { return context->aspect<Meta>().items().find(id); }
        static bool exists(Reading context, Id id) { return find(context, id) != nullptr; }
        static auto get(Reading context, Id id) -> const Quantum& {
            if (const auto* found = find(context, id)) return *found;
            utility::messages::throw_not_present("actions::get", Rtid::name<Meta>(), id.raw());
        }
        static const auto& get_global(Reading context) { return context->aspect<Meta>().global(); }
        static auto modify(Writing context, Id id) -> processing::orchestrator::QuantumGate<Meta> {
            return processing::orchestrator::QuantumGate<Meta>{context, id};
        }
        static auto modify_global(Writing context) -> processing::orchestrator::GlobalGate<Meta> {
            return processing::orchestrator::GlobalGate<Meta>{context};
        }
        // a group lives and dies with its host
        static void remove(Writing context, Id id) requires (not detail::group<Meta>) {
            context.workers_interface().updates<Meta>().put_deletion(id);
        }

        // links: ward follows custody/anchor ids, relation follows Affected; vital removes self when the target is gone
        template<typename Ward>
        static auto ward(Reading context, Id id, Identifier<Ward> Quantum::* link) -> const ::fqsm::Quantum<Ward>* {
            return Capability<Ward>::find(context, get(context, id).*link);
        }
        template<typename Ward>
        static auto ward(Reading context, Id id, std::optional<Identifier<Ward>> Quantum::* link) -> const ::fqsm::Quantum<Ward>* {
            const auto& linkId = get(context, id).*link;
            return linkId ? Capability<Ward>::find(context, *linkId) : nullptr;
        }
        template<typename Related>
        static auto relation(Reading context, Id id, ::fqsm::Affected<Related> Quantum::* link) -> const ::fqsm::Quantum<Related>* {
            return Capability<Related>::find(context, get(context, id).*link);
        }
        template<typename Related>
        static auto vital(Writing context, Id id, ::fqsm::Affected<Related> Quantum::* link) -> const ::fqsm::Quantum<Related>* {
            if (const auto* found = relation(context, id, link)) return found;
            remove(context, id);
            return nullptr;
        }

        // entity
        static auto create(Writing context, Quantum value) -> Id requires detail::entity<Meta> {
            const auto id = Id::generate_random();
            context.workers_interface().updates<Meta>().put_add(id, std::move(value));
            return id;
        }

        // attribute, feature, component, group
        static void extend(Writing context, Id id, Quantum value) requires (not detail::entity<Meta> and not detail::group<Meta>) {
            context.workers_interface().updates<Meta>().put_add(id, std::move(value));
        }
        static void extend(Writing context, Id id) requires detail::group<Meta> {
            context.workers_interface().updates<Meta>().put_add(id, {});
        }
        // kills the whole aggregate: the standalone root at the end of the host chain
        static void kraken(Writing context, Id id) requires (not detail::entity<Meta>) {
            if constexpr (meta::category::Standalone<detail::HostOf<Meta>>)
                context.workers_interface().updates<detail::HostOf<Meta>>().put_deletion(id);
            else
                meta::facade_t<detail::HostOf<Meta>>::kraken(context, id);
        }

        // group: the quantum is an IdSet of element ids, a value: a change copies the set once into the patch (one memcpy)
        static auto addElement(Writing context, Id me, typename detail::ElementOf<Meta>::Quantum element) -> typename detail::ElementOf<Meta>::Id
            requires (detail::group<Meta> and meta::category::Standalone<detail::ElementOf<Meta>>) {
            const auto elementId = Capability<detail::ElementOf<Meta>>::create(context, std::move(element));
            context.workers_interface().updates<Meta>().get_modification_access(me).insert(elementId);
            return elementId;
        }
        static auto addElement(Writing context, Id me, typename detail::ElementOf<Meta>::Id elementId, typename detail::ElementOf<Meta>::Quantum element) -> typename detail::ElementOf<Meta>::Id
            requires (detail::group<Meta> and meta::category::Parasitic<detail::ElementOf<Meta>>) {
            Capability<detail::ElementOf<Meta>>::extend(context, elementId, std::move(element));
            context.workers_interface().updates<Meta>().get_modification_access(me).insert(elementId);
            return elementId;
        }
        static void deleteElement(Writing context, Id me, typename detail::ElementOf<Meta>::Id element) requires detail::group<Meta> {
            context.workers_interface().updates<Meta>().get_modification_access(me).erase(element);
            if constexpr (meta::category::Parasitic<detail::ElementOf<Meta>>)
                Capability<detail::ElementOf<Meta>>::kraken(context, element);
            else
                Capability<detail::ElementOf<Meta>>::remove(context, element);
        }
        static void clear(Writing context, Id me) requires detail::group<Meta> {
            const Quantum elements = get(context, me);
            for (const auto& element : elements)
                deleteElement(context, me, element);
            context.workers_interface().updates<Meta>().put_modification(me, Quantum{});
        }
    };

    // Base of Meta::Internals: the reactions vocabulary of an aspect.
    template<typename Meta>
    struct Internals {
        using Reading = ::fqsm::Reading;
        using Writing = ::fqsm::Writing;
        using Reacting = ::fqsm::Reacting;
        using Retrospecting = ::fqsm::Retrospecting;
        using Behavior = features::Behavior;
        using PossibleChange = std::optional<typename Meta::Quantum>;
        using my = Capability<Meta>;
    };

    // Interpretation categories: no own state, the declaring struct is its own facade.
    template<typename Meta>
    struct Archetype : Contexts {
        using BaseActions = Meta;
    };

    template<typename Meta, meta::category::Any Primary>
    struct Manipulation : Contexts {
        using PrimaryAspect = Primary;
        using Id = typename Primary::Id;
        using Quantum = typename Primary::Quantum;
        using BaseActions = Meta;
    };
}
