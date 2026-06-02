#pragma once

#include <unordered_map>
#include <optional>

#include <base/types/patches.h>
#include <fQSM/references.h>
#include <fQSM/meta/axis.h>
#include <fQSM/meta/alias.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/meta/specializations.h>

namespace fqsm::meta::state {
    template<aspect::Any, axis::order>
    struct DataLayout;
}

namespace fqsm::meta::state {
    template<axis::mutability, typename>
    struct Reference;

    template<typename T>
    struct Reference<axis::mutability::writable, T> {
        using Type = ::fqsm::ref<T>;
    };

    template<typename T>
    struct Reference<axis::mutability::constant, T> {
        using Type = ::fqsm::cref<T>;
    };
}

namespace fqsm::meta::state {

    // Items Layout specs:
    template<aspect::Any Meta>
    struct DataLayout<Meta, axis::order::state> {
        using Item = Quantum<Meta>;
        using Global = GlobalValue<Meta>;
    };

    template<aspect::Any Meta>
    struct DataLayout<Meta, axis::order::patch> {
        using Item = base::types::Patch<Quantum<Meta>>;
        using Global = std::optional<GlobalValue<Meta>>;
    };
}


