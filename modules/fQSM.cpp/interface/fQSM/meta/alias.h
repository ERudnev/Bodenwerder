#pragma once

#include <type_traits>

#include <fQSM/meta/concepts.h> // pre-registered level mechanism

// this concepts are common:
namespace fqsm {
    namespace aspect = meta::aspect;
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
    requires meta::aspect::has::Id<Meta>
    using Id = typename Meta::Id;

    template<typename Meta>
    requires meta::aspect::has::Quantum<Meta>
    using Quantum = typename Meta::Quantum;

    template<typename Meta>
    requires meta::aspect::has::Quantum<Meta> // yes, require Quantum to allow (even empty) Global
    using GlobalValue = typename detail::aspect::GlobalValue<Meta>::Type;

}

