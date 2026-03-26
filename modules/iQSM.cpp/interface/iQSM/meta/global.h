#pragma once

#include <type_traits>

#include <iQSM/references.h>

namespace iqsm::meta {
    struct EmptyGlobal {};

    template<typename Meta>
    concept HasGlobal = requires { typename Meta::Global; };

    template<typename Meta, typename = void>
    struct global_of {
        using data = EmptyGlobal;
        using ref = cref<data>;
    };

    template<typename Meta>
    struct global_of<Meta, std::void_t<typename Meta::Global>> {
        using data = typename Meta::Global;
        using ref = cref<data>;
    };

    template<typename Meta>
    using GlobalData = typename global_of<Meta>::data;

    template<typename Meta>
    using Global = typename global_of<Meta>::ref;

    template<typename Meta>
    Global<Meta> zero_global() {
        using Data = GlobalData<Meta>;
        static const Global<Meta> singleton = base::make_shared<const Data>();
        return singleton;
    }
}

