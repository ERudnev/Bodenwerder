#pragma once

#include <type_traits>

#include <iQSM/meta/concepts.h>
#include <iQSM/references.h>

namespace iqsm {
    template<typename Meta>
    requires meta::HasId<Meta>
    using Id = typename Meta::Id;

    template<typename Meta>
    requires meta::HasQuantum<Meta>
    using Quantum = typename Meta::Quantum;

    template<typename Meta>
    requires meta::HasQuantum<Meta>
    using Item = cref<Quantum<Meta>>;

    template<typename Meta>
    requires requires { typename Meta::RuntimeStorage; }
    using RuntimeStorage = typename Meta::RuntimeStorage;

    template<typename Meta>
    requires requires { typename Meta::RuntimeAccess; }
    using RuntimeAccess = typename Meta::RuntimeAccess;
}

