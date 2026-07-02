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

#include <fQSM/manipulation/item.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/_forwards.h>
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
        static auto find(Reading, Id) -> base::maybe<std::reference_wrapper<const Quantum>>;
        static auto global(Reading) -> const Global&;

    };

    template<typename Meta>
    struct Standalone : Any<Meta> {
        using Own = Any<Meta>;

    protected:
        static Own::Id new_element(Writing context, Own::Quantum val) {
            const auto id = Identifier<Meta>::generate_random();
            context.patch().aspect<Meta>().put_add(id, std::move(val));
            return id;
        }
    };

    template<typename Meta, typename HostType>
    struct Parasitic : Any<Meta> {
        using Own = Any<Meta>;
        using Parent = Standalone<HostType>;

        // experimantal:
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

    template<typename Meta, typename HostType, typename WorkerType>
    using Manager = Parasitic<Meta, HostType>;

    // Interpretation category ations ant typedefs:
    struct Archetype : Base {
        // TODO: consider to add type lists and other stuff here
    };

}

// Impl
namespace fqsm::aspect::action {

    template<typename Meta>
    auto Any<Meta>::get(Reading context, Id id) -> const Quantum& {
        const auto found = ::fqsm::manipulation::item::get<Meta>(context, id);
        if (!found) {
            throw std::runtime_error(std::format(R"(actions::get "{}" {}: not present)", ::fqsm::meta::Rtid::name<Meta>(), id));
        }
        return found.value();
    }

    template<typename Meta>
    auto Any<Meta>::find(Reading context, Id id) -> base::maybe<std::reference_wrapper<const Quantum>> {
        return ::fqsm::manipulation::item::get<Meta>(context, id);
    }

    template<typename Meta>
    auto Any<Meta>::global(Reading context) -> const Global& {
        return context.aspect<Meta>().global();
    }

    template<typename Meta, typename HostType>
    void Parasitic<Meta, HostType>::kill(Writing context, Own::Id id) {
        if constexpr (category::Standalone<HostType>) {
            context.patch().aspect<HostType>().put_deletion(id);
        } else {
            HostType::Actions::kill(context, id);
        }
    }
}
