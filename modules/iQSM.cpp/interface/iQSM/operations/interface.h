#pragma once

#include <optional>
#include <vector>

#include <iQSM/repository/permit.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>


namespace iqsm::detail::operations {
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


namespace iqsm::detail::validation {
    // validation of any Aspect means applying Block of functions in defined order
    struct Block {
        using One = void(*)(Writing);
        using Layer = std::vector<One>;

        Layer structural;
        Layer logical;
    };
}
