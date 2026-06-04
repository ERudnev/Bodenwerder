#pragma once

// Servies are "things, that Aspects can do"

#include <optional>
#include <vector>

#include <fQSM/meta/alias.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/features/codex.h>

namespace fqsm::service {

    // each Aspect must (may?) have own functions::Interface
    class Interface {
    protected:
        virtual ~Interface() = default;
    };

    template<typename Meta>
    struct Group : Interface {
        virtual ~Group() = default;

    protected:
        using Own = Meta;
        using Id = ::fqsm::Id<Meta>;
        using Quantum = ::fqsm::Quantum<Meta>;
        using ItemChange = std::optional<Quantum>; // nullopt = no change
        using Reading = ::fqsm::Reading;
        using Writing = ::fqsm::Writing;
    };
}
