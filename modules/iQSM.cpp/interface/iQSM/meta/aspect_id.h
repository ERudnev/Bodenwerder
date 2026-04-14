#pragma once

#include <typeindex>
#include <typeinfo>

#include <iQSM/meta/concepts.h>
#include <iQSM/types.h>
#include <unordered_set>

namespace iqsm::types {
    using RuntimeId = ::iqsm::internals::Types::RuntimeId;

    template<::iqsm::meta::Aspect Meta>
    inline RuntimeId aspectId() {
        return RuntimeId(typeid(Meta));
    }
}

