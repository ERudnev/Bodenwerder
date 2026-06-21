#pragma once

#include <type_traits>

#include <fQSM/meta/categories.h> // pre-registered level mechanism
#include <fQSM/meta/rtid.h>

// this concepts are common:
namespace fqsm {
    namespace category = meta::category;
}

// default empty Aspect::Global
namespace fqsm::detail::aspect {

    namespace internals {
        struct EmptyGlobal {};
        // TODO: consider to allow Quantum to be not defined: struct EmptyQuantum {};
    }

    template<typename Meta, typename = void>
    struct GlobalValue {
        using Type = internals::EmptyGlobal;
    };
    template<typename Meta>
    struct GlobalValue<Meta, std::void_t<typename Meta::Global>> {
        using Type = typename Meta::Global;
    };
}

namespace fqsm {
    template<typename Meta>
    requires meta::category::musthave::Id<Meta>
    using Id = typename Meta::Id;

    template<typename Meta>
    requires meta::category::musthave::Quantum<Meta>
    using Quantum = typename Meta::Quantum;

    template<typename Meta>
    requires meta::category::musthave::Quantum<Meta> // yes, require Quantum to allow (even empty) Global
    using GlobalValue = typename detail::aspect::GlobalValue<Meta>::Type;

    template<typename Meta>
    requires meta::category::Any<Meta>
    inline const meta::Rtid TypeId = meta::Rtid::of<Meta>();

}
