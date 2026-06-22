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
#include <fQSM/features/codex.h>


// rename to fqsm::actions::categories {
namespace fqsm::actions {
    // each Aspect must (may?) have own Interface
    // this set of interfaces is used to generate Aspect-specific parts of their interfaces
    struct Base {
        using Reading = ::fqsm::Reading;
        using Writing = ::fqsm::Writing;
        template<meta::category::Any Meta>
        using Direct = ::fqsm::Direct<Meta>;
    };

    template<typename Meta>
    struct Any : Base {
        friend class ::fqsm::features::Codex;

        // basic alias
        using Id = ::fqsm::Id<Meta>;
        using Quantum = ::fqsm::Quantum<Meta>;
        using Global = ::fqsm::GlobalValue<Meta>;
        using Update = std::optional<Quantum>;

        // signatures
        // read-only:
        using QuantumLocal = Update(*)(const Quantum&); // new quantum value can be evaluated only from old one
        using QuantumDependent = Update(*)(Reading, Id, const Quantum&); // Id is major, but basic stuff uses only Quantum

        // read-write
        using Action = void(*)(Writing, Id, const Quantum&); // abstract "action" - "do something with specific element"

        // helpers:
        static auto get(Reading, Id) -> const Quantum&;
        static auto find(Reading, Id) -> base::maybe<std::reference_wrapper<const Quantum>>;
        static auto global(Reading) -> const Global&;

    };

    template<typename Meta>
    struct Standalone : Any<Meta> {
        using Own = Any<Meta>;
    };

    template<typename Meta, typename HostType>
    struct Parasitic : Any<Meta> {
        using Own = Any<Meta>;
        using Parent = Standalone<HostType>;

        using ConstructFromParent = void(*)(Writing, typename Parent::Id);

        // feature:
        static void kill(Writing context, Own::Id id);
    };


    template<typename Meta>
    using Entity = Standalone<Meta>;


    template<typename Meta, typename WorkerType> // WorkerType is not used yet
    using Controller = Standalone<Meta>;


    template<typename Meta, typename HostType>
    using Attribute = Parasitic<Meta, HostType>;


    template<typename Meta, typename HostType>
    using Component = Parasitic<Meta, HostType>;
}

// Impl
namespace fqsm::actions {

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
            manipulation::item::update<HostType>(context, id).remove();
        } else {
            HostType::Actions::kill(context, id);
        }
    }
}
