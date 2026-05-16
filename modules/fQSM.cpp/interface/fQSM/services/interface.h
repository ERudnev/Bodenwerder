#pragma once

#include <optional>
#include <vector>

#include <fQSM/meta/alias.h>

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
    };
}
