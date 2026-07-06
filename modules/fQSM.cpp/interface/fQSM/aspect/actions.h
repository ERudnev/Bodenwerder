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

#include <optional>
#include <vector>

#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/transactions/quantal.h>
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

        // basic alias
        using Id = ::fqsm::Id<Meta>;
        //experiment: switch do state:: as language of types: using Quantum = ::fqsm::model::
        using Quantum = ::fqsm::Quantum<Meta>;
        using Global = ::fqsm::GlobalValue<Meta>;
        using PossibleChange = std::optional<Quantum>; // use model::elementary::Patch here

        // signatures
        struct Vocabulary {
            using EvaluateQuantumLocal = std::function<PossibleChange(const Quantum&)>; // Quantum -> Quantum possible modification
            using EvaluateQuantumContextual = std::function<PossibleChange(Reading, Id, const Quantum&)>; // Id is main channel, Quantum& is a cache
            using JustWriting = std::function<void(Writing, Id, const Quantum&)>;
        };
        // replaces with Func::
        //using QuantumLocal = std::function<PossibleChange(const Quantum&)>;
        //using QuantumDependent = std::function<PossibleChange(Reading, Id, const Quantum&)>; // Id is major, but basic stuff uses only Quantum
        //using Elementary = std::function<void(Writing, Id, const Quantum&)>;

        // elementary (func):
        static auto count(Reading) -> size_t;
        static auto get(Reading, Id) -> const Quantum&;
        static auto find(Reading, Id) -> const Quantum*;
        static bool exists(Reading, Id);
        static auto get_global(Reading) -> const Global&;
        static void remove(Writing, Id);
        // elementary (RAII):
        static auto modify(Writing, Id) -> ::fqsm::processing::transaction::Quantal<Meta>;
        static auto modify_global(Writing) -> ::fqsm::processing::transaction::Global<Meta>;
    };


    // Standalone (Entities..) have this stuff:
    template<typename Meta>
    struct Standalone : Any<Meta> {
        using Id = typename Any<Meta>::Id;
        using Quantum = typename Any<Meta>::Quantum;

        // elementary (func):
        static Id create(Writing context, Quantum val);
    };


    template<typename Meta, typename HostType>
    struct Parasitic : Any<Meta> {
        using Id = typename Any<Meta>::Id;
        using Quantum = typename Any<Meta>::Quantum;
        using Parent = Standalone<HostType>;

        static void create_for(Writing context, Id id, Quantum val);

        // experimental: kills parasite AND its host
        static void kraken(Writing context, Id id);
    };


    template<typename Meta>
    struct Entity : Standalone<Meta> {};

    template<typename Meta, typename HostType>
    struct Attribute : Parasitic<Meta, HostType> {};

    template<typename Meta, typename HostType>
    struct Component : Parasitic<Meta, HostType> {};

    template<typename Meta, typename HostType, category::Entity ElementType>
    struct Group : Parasitic<Meta, HostType> { //, protected ElementType::Actions {
        using Own = Parasitic<Meta, HostType>::Own;
        using Parent = Parasitic<Meta, HostType>::Parent;
        using Client = ElementType;

        // like public interface:
        static void create_for(Writing, Id<HostType>); // simplier version of Parasitic::create_for
        static auto addElement(Writing, Own::Id me, Client::Quantum) -> Client::Id;
        // static auto addElement(Writing, Own::Id me, &func, Args...) -> Client::Id;
        static void deleteElement(Writing, Own::Id me, Client::Id);
    private:
        using Parasitic<Meta, HostType>::create_for;
    };

    // Interpretation category ations ant typedefs:
    struct Archetype : Base {
        // TODO: consider to add type lists and other stuff here
    };

}

// Impl
namespace fqsm::aspect::actions {

    //
    // Any
    template<typename Meta>
    auto Any<Meta>
    ::count(Reading context)
    -> size_t {
        return context.aspect<Meta>().items().size();
    }


    template<typename Meta>
    auto Any<Meta>
    ::get(Reading context, Id id)
    -> const Quantum& {
        const auto* found = context.aspect<Meta>().items().find(id);
        if (!found) {
            throw std::runtime_error(std::format(R"(actions::get "{}" {}: not present)", ::fqsm::meta::Rtid::name<Meta>(), id));
        }
        return *found;
    }

    template<typename Meta>
    auto Any<Meta>
    ::find(Reading context, Id id)
    ->const Quantum*{
        return context.aspect<Meta>().items().find(id);
    }

    template<typename Meta>
    bool Any<Meta>
    ::exists(Reading context, Id id) {
        return context.aspect<Meta>().items().find(id) != nullptr;
    }

    template<typename Meta>
    auto Any<Meta>
    ::get_global(Reading context)
    ->const Global&
    {
        return context.aspect<Meta>().global();
    }

    template<typename Meta>
    void Any<Meta>
    ::remove(Writing context, Id id) {
        context.workers_interface().updates<Meta>().put_deletion(id);
    }

    template<typename Meta>
    auto Any<Meta>
    ::modify(Writing context, Id id)
    -> ::fqsm::processing::transaction::Quantal<Meta> {
        return ::fqsm::processing::transaction::Quantal<Meta>{context, id};
    }

    template<typename Meta>
    auto Any<Meta>
    ::modify_global(Writing context)
    -> ::fqsm::processing::transaction::Global<Meta> {
        return ::fqsm::processing::transaction::Global<Meta>{context};
    }

    //
    // Standalone:
    template<typename Meta>
    auto Standalone<Meta>
    ::create(Writing context, Quantum val)
    ->Id {
        const auto id = Identifier<Meta>::generate_random();
        context.workers_interface().updates<Meta>().put_add(id, std::move(val));
        return id;
    }

    //
    // Parasitic:
    template<typename Meta, typename HostType>
    void Parasitic<Meta, HostType>
    ::create_for(Writing context, Id id, Quantum val) {
        context.workers_interface().updates<Meta>().put_add(id, std::move(val));
    }

    template<typename Meta, typename HostType>
    void Parasitic<Meta, HostType>
    ::kraken(Writing context, Id id) {
        if constexpr (category::Standalone<HostType>) {
            context.workers_interface().updates<HostType>().put_deletion(id);
        } else {
            HostType::Actions::kraken(context, id);
        }
    }

    //
    // Group:
    template<typename Meta, typename HostType, category::Entity ElementType>
    void Group<Meta, HostType, ElementType>
    ::create_for(Writing context, Id<HostType> id) {
        context.workers_interface().updates<Meta>().put_add(id, {});
    }


    template<typename Meta, typename HostType, category::Entity ElementType>
    auto Group<Meta, HostType, ElementType>
    ::addElement(Writing context, Own::Id myId, Client::Quantum element)
    ->Client::Id {
        const auto workerId = Client::Actions::create(context, std::move(element));
        // this is very important place.
        // place where fQSM, even DAQL and Q1 may become recursive.
        // current "Group" is object with Fat Quantum (set of id's)
        // each mutation of this set must treat is as immutable object (copy to change)
        // this hits limits of fQSM, where Quantum can not be System itself.
        // it is realy big story of recursive ECS where Component mey be a System of inner Components
        // So... lets sacrifice performance to avoid this stuff.
        auto myQuantum = context->aspect<Meta>().items().at(myId);
        myQuantum.insert(workerId);
        context.workers_interface().updates<Meta>().put_modification(myId, std::move(myQuantum));
        return workerId;
    }

    template<typename Meta, typename HostType, category::Entity ElementType>
    void Group<Meta, HostType, ElementType>
    ::deleteElement(Writing context, Own::Id myId, Client::Id worker) {
        auto myQuantum = context->aspect<Meta>().items().at(myId);
        myQuantum.erase(worker);
        context.workers_interface().updates<Meta>().put_modification(myId, std::move(myQuantum));
        context.workers_interface().updates<ElementType>().put_deletion(worker);
    }
}
