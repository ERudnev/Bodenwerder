#pragma once

#include <iQSM/internals/type_list.h>

namespace iqsm {
    template<typename... Deps>
    struct Require {
        using Depends = internals::type_list<Deps...>;
    };
}

