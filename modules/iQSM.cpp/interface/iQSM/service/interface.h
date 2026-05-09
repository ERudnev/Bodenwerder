#pragma once

#include <optional>
#include <vector>

#include <iQSM/meta/concepts.h>
#include <iQSM/meta/alias.h>
#include <iQSM/flow/interface.h>
#include <iQSM/state/_forwards.h>


namespace iqsm::service {

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
        using Id = ::iqsm::Id<Meta>;
        using Quantum = ::iqsm::Quantum<Meta>;
        using ItemChange = std::optional<Quantum>; // nullopt = no change
    };
}
