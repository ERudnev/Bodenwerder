#pragma once

#include <type_traits>

#include <iQSM/meta/concepts.h>
#include <iQSM/references.h>


// default empty Aspect::Global
namespace iqsm::detail::aspects {
    struct EmptyGlobal {};

    template<typename Meta, typename = void>
    struct GlobalValue {
        using Type = EmptyGlobal;
    };
    template<typename Meta>
    struct GlobalValue<Meta, std::void_t<typename Meta::Global>> {
        using Type = typename Meta::Global;
    };
}

namespace iqsm {
    template<typename Meta>
    requires meta::HasId<Meta>
    using Id = typename Meta::Id;

    template<typename Meta>
    requires meta::HasQuantum<Meta>
    using Quantum = typename Meta::Quantum;

    template<typename Meta>
    requires meta::HasQuantum<Meta>
    using Node = cref<Quantum<Meta>>;

    template<typename Meta>
    requires meta::HasQuantum<Meta> // yes, require Quantum to allow (even empty) Global
    using GlobalValue = typename detail::aspects::GlobalValue<Meta>::Type;

    template<typename Meta>
    requires meta::HasQuantum<Meta>
    using Global = cref<GlobalValue<Meta>>;
}

