#pragma once

// Capabilites is set of features of Aspect, similar to methods of objects
// like "do something()" "what is your size()"
// fQSM-specific fact: all "methods" of Aspect are implementated as static methods of Aspect::Capabilities interface
// notmal shape:
// static auto Aspect::Capabilities::foo(Reading, Id, Args...)->result
// static auto Aspect::Capabilities::bar(Writing, Id, Args...)->result
// true statics (methods of Aspect itself, not its Items:
// static auto Aspect::Capabilities::static_foo(Reading, Args...)->result
// static auto Aspect::Capabilities::static_bar(Writing, Args...)->result

#include <optional>
#include <vector>

#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/features/codex.h>

namespace fqsm::capabilities {
    // each Aspect must (may?) have own Interface
    // this set of interfaces is used to generate Aspect-specific parts of their interfaces
    class Abstract {
    protected:
        virtual ~Abstract() = default;

        using Reading = ::fqsm::Reading;
        using Writing = ::fqsm::Writing;
    };


    template<typename Meta>
    struct Any : Abstract {
        virtual ~Any() = default;

    protected:
        using Own = Meta;
        using Id = ::fqsm::Id<Meta>;
        using Quantum = ::fqsm::Quantum<Meta>;
        using ItemChange = std::optional<Quantum>; // nullopt = no change
    };


    template<typename Meta>
    using Standalone = Any<Meta>;

    template<typename Meta, typename HostType>
    struct Parasitic : Any<Meta> {
    public:
        using AutoConstructorType = void(Writing, ::fqsm::Id<HostType>);
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
