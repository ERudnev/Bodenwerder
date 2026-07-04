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
namespace fqsm::aspect::action {
    // each Aspect must (may?) have own Interface
    // this set of interfaces is used to generate Aspect-specific parts of their interfaces
    struct Base {
        using Reading = ::fqsm::Reading;
        using Writing = ::fqsm::Writing;
        template<meta::category::Any Meta>
        using Direct = ::fqsm::Direct<Meta>;
        // TODO: remove this after Behavior incapsulated in Meta as local structure
        //using Reacting = ::fqsm::Reacting; // this enables Reactions role for Actions and reqires separation
    };

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
        // read-only:
        using QuantumLocal = std::function<PossibleChange(const Quantum&)>; // new quantum value can be evaluated only from old one
        using QuantumDependent = std::function<PossibleChange(Reading, Id, const Quantum&)>; // Id is major, but basic stuff uses only Quantum

        // read-write
        using Elementary = std::function<void(Writing, Id, const Quantum&)>;

        // helpers:
        static auto get(Reading, Id) -> const Quantum&;
        static const Quantum* find(Reading, Id);
        static bool exists(Reading, Id);
        static auto global(Reading) -> const Global&;
        static void global_set(Writing, Global);
        static void remove(Writing, Id);

        using Modify = ::fqsm::processing::transaction::Quantal<Meta>;
        static Modify modify(Writing context, Id id) {
            return Modify{context, id};
        }

    };

    template<typename Meta>
    struct Standalone : Any<Meta> {
        using Own = Any<Meta>;

        // used in other aspets. May be entry point for "make dependencies by deriving Action interface"

        // rename to just "create" after API cleanup
        static Own::Id create_new(Writing context, Own::Quantum val) {
            const auto id = Identifier<Meta>::generate_random();
            context.patch().aspect<Meta>().put_add(id, std::move(val));
            return id;
        }
    };

    template<typename Meta, typename HostType>
    struct Parasitic : Any<Meta> {
        using Own = Any<Meta>;
        using Parent = Standalone<HostType>;

        static void create_for(Writing context, Own::Id id, Own::Quantum val) {
            context.patch().aspect<Meta>().put_add(id, std::move(val));
        }

        // experimental:
        static void kill(Writing context, Own::Id id);
    protected:
        // derived actions helpers:
        static void new_element(Writing context, Own::Id id, Own::Quantum val) { context.patch().aspect<Meta>().put_add(id, std::move(val)); }
    };


    template<typename Meta>
    using Entity = Standalone<Meta>;

    template<typename Meta, typename HostType>
    using Attribute = Parasitic<Meta, HostType>;

    template<typename Meta, typename HostType>
    using Component = Parasitic<Meta, HostType>;

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
    };

    // Interpretation category ations ant typedefs:
    struct Archetype : Base {
        // TODO: consider to add type lists and other stuff here
    };

}

// Impl
namespace fqsm::aspect::action {

    template<typename Meta>
    auto Any<Meta>
    ::get(Reading context, Id id) -> const Quantum& {
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
    ::global(Reading context)
    ->const Global&
    {
        return context.aspect<Meta>().global();
    }

    template<typename Meta>
    void Any<Meta>
    ::global_set(Writing context, Global val) {
        //context.patch().aspect<Meta>().global = {std::move(val)};
        context.workers_interface().updates<Meta>().put_global(std::move(val));
    }



    template<typename Meta, typename HostType>
    void Parasitic<Meta, HostType>
    ::kill(Writing context, Own::Id id) {
        if constexpr (category::Standalone<HostType>) {
            context.patch().aspect<HostType>().put_deletion(id);
        } else {
            HostType::Actions::kill(context, id);
        }
    }

    template<typename Meta>
    void Any<Meta>
    ::remove(Writing context, Id id) {
        context.patch().aspect<Meta>().put_deletion(id);
    }


    // Group:
    template<typename Meta, typename HostType, category::Entity ElementType>
    void Group<Meta, HostType, ElementType>
    ::create_for(Writing context, Id<HostType> id) {
        context.patch().aspect<Meta>().put_add(id, {});
    }


    template<typename Meta, typename HostType, category::Entity ElementType>
    auto Group<Meta, HostType, ElementType>
    ::addElement(Writing context, Own::Id myId, Client::Quantum element)
    ->Client::Id {
        const auto workerId = Client::Actions::create_new(context, std::move(element));

        // this is very impletant place.
        // place where fQSM, even DAQL and Q1 may become recursive.
        // current "Group" is object with Fat Quantum (set of id's)
        // each mutation of this set must treat is as immutable object (copy to change)
        // this hits limits of fQSM, where Quantum can not be System itself.
        // it is realy big story of recursive ECS where Component mey be a System of inner Components
        // So... lets sacrifice performance to avoid this stuff.
        auto myQuantum = context->aspect<Meta>().items().at(myId);
        myQuantum.insert(workerId);
        context.patch().aspect<Meta>().put_modification(myId, std::move(myQuantum));
        return workerId;
    }

    template<typename Meta, typename HostType, category::Entity ElementType>
    void Group<Meta, HostType, ElementType>
    ::deleteElement(Writing context, Own::Id myId, Client::Id worker) {
        auto myQuantum = context->aspect<Meta>().items().at(myId);
        myQuantum.erase(worker);
        context.patch().aspect<Meta>().put_modification(myId, std::move(myQuantum));
        context.patch().aspect<ElementType>().put_deletion(worker);
    }
}
