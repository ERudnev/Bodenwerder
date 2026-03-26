#pragma once

#include <typeindex>
#include <typeinfo>

#include <iQSM/meta/concepts.h>
#include <iQSM/types.h>

namespace iqsm::types {
    using RuntimeId = ::iqsm::internals::Types::RuntimeId;

    template<::iqsm::meta::Aspect Meta>
    inline RuntimeId aspectId() {
        return RuntimeId(typeid(Meta));
    }
}

