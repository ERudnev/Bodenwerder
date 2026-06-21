#pragma once

#include <type_traits>

#include <fQSM/meta/categories.h>

namespace fqsm::manipulation::detail {
    template<typename Meta, typename = void>
    struct actions {
        using type = typename Meta::BaseActions;
    };

    template<typename Meta>
    struct actions<Meta, std::void_t<typename Meta::Actions>> {
        using type = typename Meta::Actions;
    };
}

namespace fqsm::manipulation {
    template<typename Meta>
    using call = typename detail::actions<Meta>::type;
}