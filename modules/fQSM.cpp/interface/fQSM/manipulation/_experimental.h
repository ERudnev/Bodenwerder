#pragma once

#include <fQSM/meta/alias.h>

namespace fqsm::manipulation {
    template<typename Meta>
    using call_action = meta::facade_t<Meta>;
}
