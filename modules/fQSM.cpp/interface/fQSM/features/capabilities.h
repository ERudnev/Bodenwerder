#pragma once

// Servies are "things, that Aspects can do"

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
