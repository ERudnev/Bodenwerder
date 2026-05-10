#pragma once

#include <iQSM/meta/type_list.h>

namespace iqsm::meta {
    template<typename... Deps>
    struct Require {
        using Depends = internals::type_list<Deps...>;
    };
}
