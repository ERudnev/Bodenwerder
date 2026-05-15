#pragma once

#include <type_traits>

#include <iQSM/meta/concepts/archetype.h> // pre-registered level mechanism
#include <iQSM/references.h>


// default empty Aspect::Global
namespace iqsm::detail::aspects {

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

namespace iqsm {
    template<typename Meta>
    requires meta::archetype::has::Id<Meta>
    using Id = typename Meta::Id;

    template<typename Meta>
    requires meta::archetype::has::Quantum<Meta>
    using Quantum = typename Meta::Quantum;

    template<typename Meta>
    requires meta::archetype::has::Quantum<Meta>
    using Node = cref<Quantum<Meta>>;

    template<typename Meta>
    requires meta::archetype::has::Quantum<Meta> // yes, require Quantum to allow (even empty) Global
    using GlobalValue = typename detail::aspects::GlobalValue<Meta>::Type;

    template<typename Meta>
    requires meta::archetype::has::Quantum<Meta>
    using Global = cref<GlobalValue<Meta>>;
}

