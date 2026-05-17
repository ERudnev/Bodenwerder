#pragma once

#include <fQSM/meta/type_list.h>

namespace fqsm::meta {
    template<typename... Deps>
    struct Require {
        using Depends = internals::type_list<Deps...>;
    };
}
