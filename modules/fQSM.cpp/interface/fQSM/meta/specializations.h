#pragma once

#include <unordered_map>
#include <optional>

#include <base/maybe.h>
#include <fQSM/typeId.h>
#include <fQSM/references.h>
#include <fQSM/meta/axis.h>
#include <fQSM/meta/alias.h>
#include <fQSM/meta/concepts.h>

namespace fqsm::meta::state {
    template<aspect::Any, axis::order>
    struct ItemsLayout;
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
    struct ItemsLayout<Meta, axis::order::state> {
        using Element = Quantum<Meta>;
    };

    template<aspect::Any Meta>
    struct ItemsLayout<Meta, axis::order::patch> {
        using Element = base::maybe<Quantum<Meta>>;
    };
}


