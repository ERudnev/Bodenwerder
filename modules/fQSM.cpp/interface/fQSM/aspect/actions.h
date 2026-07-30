#pragma once

// Capabilites is set of features of Aspect, similar to methods of objects
// like "do something()" "what is your size()"
// fQSM-specific fact: all "methods" of Aspect are implementated as static methods of Aspect::Actions interface
// notmal shape:
// static auto Aspect::Actions::foo(Reading, Id, Args...)->result
// static auto Aspect::Actions::bar(Writing, Id, Args...)->result
// true statics (methods of Aspect itself, not its Items:
// static auto Aspect::Actions::static_foo(Reading, Args...)->result
// static auto Aspect::Actions::static_bar(Writing, Args...)->result
//
// Library ladder builds nested Capability; category facade is `my`
// (BaseActions / with<> / Internals::my → that facade).

#include <optional>
#include <vector>

#include <fQSM/meta/interface.include.h>
#include <fQSM/identifier.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/orchestrators/quantal.h>
#include <fQSM/features/behavior.h>

// rename to fqsm::actions::categories {
namespace fqsm::aspect::actions {
    // each Aspect must (may?) have own Interface
    // this set of interfaces is used to generate Aspect-specific parts of their interfaces
    struct Base {
        using Reading = ::fqsm::Reading;
        using Writing = ::fqsm::Writing;
        template<meta::category::Any Meta>
        using Direct = ::fqsm::Direct<Meta>;
    };

    //
    // Any Aspect class has this stuff:
    template<typename Meta>
    struct Any : Base {
        friend class ::fqsm::features::Behavior;

        struct Capability : Base {
            friend class ::fqsm::features::Behavior;

            using Id = ::fqsm::Id<Meta>;
            using Quantum = ::fqsm::Quantum<Meta>;
            using Global = ::fqsm::GlobalValue<Meta>;
            using PossibleChange = std::optional<Quantum>;
            using my = Capability; // risky thing, if something is broken... think about it

            struct Vocabulary {
                using EvaluateQuantumLocal = std::function<PossibleChange(const Quantum&)>;
                using EvaluateQuantumContextual = std::function<PossibleChange(Reading, Id, const Quantum&)>;
                using JustWriting = std::function<void(Writing, Id, const Quantum&)>;
                using JustRetrospecting = std::function<void(Retrospecting, Id, const Quantum&)>;
            };

            static auto count(Reading) -> size_t;
            static auto get(Reading, Id) -> const Quantum&;
            static auto find(Reading, Id) -> const Quantum*;

            template<typename Ward>
            static auto ward(Reading context, Id id, ::fqsm::Identifier<Ward> Quantum::* link)
                -> const ::fqsm::Quantum<Ward>* {
                return Any<Ward>::Capability::find(context, get(context, id).*link);
            }

            template<typename Ward>
            static auto ward(Reading context, Id id, std::optional<::fqsm::Identifier<Ward>> Quantum::* link)
                -> const ::fqsm::Quantum<Ward>* {
                const auto& linkId = get(context, id).*link;
                if (not linkId)
                    return nullptr;
                return Any<Ward>::Capability::find(context, *linkId);
            }

            template<typename Related>
            static auto relation(Reading context, Id id, ::fqsm::Affected<Related> Quantum::* link)
                -> const ::fqsm::Quantum<Related>* {
                return Any<Related>::Capability::find(context, get(context, id).*link);
            }

            // Vital Affected: miss → remove(self), nullptr.
            template<typename Related>
            static auto vital(Writing context, Id id, ::fqsm::Affected<Related> Quantum::* link)
                -> const ::fqsm::Quantum<Related>* {
                if (const auto* found = relation(context, id, link))
                    return found;
                remove(context, id);
                return nullptr;
            }

            static bool exists(Reading, Id);
            static auto get_global(Reading) -> const Global&;
            static void remove(Writing, Id);
            static auto modify(Writing, Id) -> ::fqsm::processing::orchestrator::QuantumGate<Meta>;
            static auto modify_global(Writing) -> ::fqsm::processing::orchestrator::GlobalGate<Meta>;
        };
    };


    // Standalone (Entities..) have this stuff:
    template<typename Meta>
    struct Standalone {
        struct Capability : Any<Meta>::Capability {
            using Id = typename Any<Meta>::Capability::Id;
            using Quantum = typename Any<Meta>::Capability::Quantum;

            static Id create(Writing context, Quantum val);
        };
    };


    template<typename Meta, typename HostType>
    struct Parasitic {
        struct Capability : Any<Meta>::Capability {
            using Id = typename Any<Meta>::Capability::Id;
            using Quantum = typename Any<Meta>::Capability::Quantum;
            using Parent = typename Standalone<HostType>::Capability;

            static void extend(Writing context, Id id, Quantum val);
            static void kraken(Writing context, Id id);
        };
    };


    template<typename Meta>
    struct Entity {
        using my = typename Standalone<Meta>::Capability;
    };

    template<typename Meta, typename HostType>
    struct Attribute {
        using my = typename Parasitic<Meta, HostType>::Capability;
    };

    template<typename Meta, typename HostType>
    struct Feature {
        using my = typename Parasitic<Meta, HostType>::Capability;
    };

    template<typename Meta, typename HostType>
    struct Component {
        using my = typename Parasitic<Meta, HostType>::Capability;
    };

    template<typename Meta, typename HostType, category::Any ElementType>
    struct Group {
        using Client = ElementType;

        struct my : Parasitic<Meta, HostType>::Capability {
            using Id = typename Parasitic<Meta, HostType>::Capability::Id;

            static void extend(Writing, ::fqsm::Id<HostType>); // simpler version of Parasitic::extend
            static auto addElement(Writing, Id me, Client::Quantum) -> Client::Id requires category::Standalone<Client>;
            static auto addElement(Writing, Id me, Client::Id worker, Client::Quantum) -> Client::Id requires category::Parasitic<Client>;
            static void deleteElement(Writing, Id me, Client::Id);
            static void clear(Writing, Id me);
        private:
            using Parasitic<Meta, HostType>::Capability::extend;
            using Parasitic<Meta, HostType>::Capability::remove;
        };
    };

    // Interpretation category ations ant typedefs:
    struct Archetype : Base {
        // TODO: consider to add type lists and other stuff here
    };

    struct Manipulation : Base {
        // operation-only facade; Id/Quantum aliases come from PrimaryAspect in assembly layer
    };

}

// Impl
namespace fqsm::aspect::actions {

    //
    // Any::Capability
    template<typename Meta>
    auto Any<Meta>::Capability
    ::count(Reading context)
    -> size_t {
        return context->aspect<Meta>().items().size();
    }


    template<typename Meta>
    auto Any<Meta>::Capability
    ::get(Reading context, Id id)
    -> const Quantum& {
        const auto* found = context->aspect<Meta>().items().find(id);
        if (!found) {
            throw std::runtime_error(std::format(R"(actions::get "{}" {}: not present)", ::fqsm::meta::Rtid::name<Meta>(), id));
        }
        return *found;
    }

    template<typename Meta>
    auto Any<Meta>::Capability
    ::find(Reading context, Id id)
    ->const Quantum*{
        return context->aspect<Meta>().items().find(id);
    }

    template<typename Meta>
    bool Any<Meta>::Capability
    ::exists(Reading context, Id id) {
        return context->aspect<Meta>().items().find(id) != nullptr;
    }

    template<typename Meta>
    auto Any<Meta>::Capability
    ::get_global(Reading context)
    ->const Global&
    {
        return context->aspect<Meta>().global();
    }

    template<typename Meta>
    void Any<Meta>::Capability
    ::remove(Writing context, Id id) {
        context.workers_interface().updates<Meta>().put_deletion(id);
    }

    template<typename Meta>
    auto Any<Meta>::Capability
    ::modify(Writing context, Id id)
    -> ::fqsm::processing::orchestrator::QuantumGate<Meta> {
        return ::fqsm::processing::orchestrator::QuantumGate<Meta>{context, id};
    }

    template<typename Meta>
    auto Any<Meta>::Capability
    ::modify_global(Writing context)
    -> ::fqsm::processing::orchestrator::GlobalGate<Meta> {
        return ::fqsm::processing::orchestrator::GlobalGate<Meta>{context};
    }

    //
    // Standalone::Capability
    template<typename Meta>
    auto Standalone<Meta>::Capability
    ::create(Writing context, Quantum val)
    ->Id {
        const auto id = Identifier<Meta>::generate_random();
        context.workers_interface().updates<Meta>().put_add(id, std::move(val));
        return id;
    }

    //
    // Parasitic::Capability
    template<typename Meta, typename HostType>
    void Parasitic<Meta, HostType>::Capability
    ::extend(Writing context, Id id, Quantum val) {
        context.workers_interface().updates<Meta>().put_add(id, std::move(val));
    }

    template<typename Meta, typename HostType>
    void Parasitic<Meta, HostType>::Capability
    ::kraken(Writing context, Id id) {
        if constexpr (category::Standalone<HostType>) {
            context.workers_interface().updates<HostType>().put_deletion(id);
        } else {
            HostType::Actions::kraken(context, id);
        }
    }

    //
    // Group::my (facade)
    template<typename Meta, typename HostType, category::Any ElementType>
    void Group<Meta, HostType, ElementType>::my
    ::extend(Writing context, ::fqsm::Id<HostType> id) {
        context.workers_interface().updates<Meta>().put_add(id, {});
    }


    template<typename Meta, typename HostType, category::Any ElementType>
    auto Group<Meta, HostType, ElementType>::my
    ::addElement(Writing context, Id myId, Client::Quantum element)
    ->Client::Id
    requires category::Standalone<Client> {
        const auto workerId = Client::BaseActions::create(context, std::move(element));
        // this is very important place.
        // place where fQSM, even DAQL and Q1 may become recursive.
        // current "Group" is object with Fat Quantum (set of id's)
        // each mutation of this set must treat is as immutable object (copy to change)
        // this hits limits of fQSM, where Quantum can not be System itself.
        // it is realy big story of recursive ECS where Component mey be a System of inner Components
        // So... lets sacrifice performance to avoid this stuff.

        // upd: something was fixed with this (updated Patchlets)
        auto& myQuantum = context.workers_interface().updates<Meta>().get_modification_access(myId);
        myQuantum.insert(workerId);
        return workerId;
    }

    template<typename Meta, typename HostType, category::Any ElementType>
    auto Group<Meta, HostType, ElementType>::my
    ::addElement(Writing context, Id myId, Client::Id workerId, Client::Quantum element)
    ->Client::Id
    requires category::Parasitic<Client> {
        Client::BaseActions::extend(context, workerId, std::move(element));
        auto& myQuantum = context.workers_interface().updates<Meta>().get_modification_access(myId);
        myQuantum.insert(workerId);
        return workerId;
    }

    template<typename Meta, typename HostType, category::Any ElementType>
    void Group<Meta, HostType, ElementType>::my
    ::deleteElement(Writing context, Id myId, Client::Id worker) {
        // TODO: call kraken if managed ElementType is Parasitic it its Actions have ::kraken() func
        auto& myQuantum = context.workers_interface().updates<Meta>().get_modification_access(myId);
        myQuantum.erase(worker);
        if constexpr (category::Parasitic<Client>) {
            Client::BaseActions::kraken(context, worker);
        } else {
            Client::BaseActions::remove(context, worker);
        }
    }

    template<typename Meta, typename HostType, category::Any ElementType>
    void Group<Meta, HostType, ElementType>::my
    ::clear(Writing context, Id myId) {
        const auto& myList = context->aspect<Meta>().items().at(myId);
        for (auto& element : myList )
            deleteElement(context, myId, element);
        context.workers_interface().updates<Meta>().put_modification(myId, typename Meta::Quantum({}));
    }
}
