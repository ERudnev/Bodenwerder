#pragma once

#include <map>
#include <optional>

#include <iQSM/typeId.h>
#include <iQSM/meta/alias.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/state/_forwards.h>
#include <iQSM/state/patches.h>


namespace iqsm::state::detail {
    template<meta::Aspect Meta, policy::versioning, policy::role>
    struct ItemsLayout;

    template<policy::versioning>
    struct SlicesLayout;
}

namespace iqsm::state {
    template<meta::Aspect Meta, policy::versioning ItemsVersioning, policy::role ItemRole>
    using Item = typename detail::ItemsLayout<Meta, ItemsVersioning, ItemRole>::Element;

    template<policy::versioning SliceVersioning>
    using SlicesLayout = detail::SlicesLayout<SliceVersioning>;
    
    //template<policy::versioning OperationalVersioning>
}


namespace iqsm::state::detail {
    template<meta::Aspect Meta>
    struct ItemsLayout<Meta, policy::versioning::shared, policy::role::value> {
        using Element = Node<Meta>;
    };

    template<meta::Aspect Meta>
    struct ItemsLayout<Meta, policy::versioning::shared, policy::role::patch> {
        using Element = VersionedPatch<Meta>;
    };

    template<meta::Aspect Meta>
    struct ItemsLayout<Meta, policy::versioning::single, policy::role::value> {
        using Element = Quantum<Meta>;
    };

    template<meta::Aspect Meta>
    struct ItemsLayout<Meta, policy::versioning::single, policy::role::patch> {
        using Element = FlatPatch<Meta>;
    };

    template<>
    struct SlicesLayout<policy::versioning::shared> {  
        template<typename T>
        using RefQualified = iqsm::cref<T>;
        using SlicesContainer = std::map<RAId, RefQualified<slice::Abstract>>; // TODO: std::map -> base::DenseTable
    };

    template<>
    struct SlicesLayout<policy::versioning::single> {
        template<typename T>
        using RefQualified = iqsm::ref<T>;
        using SlicesContainer = std::map<RAId, RefQualified<slice::Abstract>>; // TODO: std::map -> base::DenseTable
    };
}