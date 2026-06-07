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
    class Abstract {
    protected:
        virtual ~Abstract() = default;
        
        using Reading = ::fqsm::Reading;
        using Writing = ::fqsm::Writing;
    };

    // TODO: rename
    template<typename Meta>
    struct Aspect : Abstract {
        virtual ~Aspect() = default;

    protected:
        using Own = Meta;
        using Id = ::fqsm::Id<Meta>;
        using Quantum = ::fqsm::Quantum<Meta>;
        using ItemChange = std::optional<Quantum>; // nullopt = no change
    };
}
