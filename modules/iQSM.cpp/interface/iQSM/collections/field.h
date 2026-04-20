#pragma once

#include <iQSM/types.h>

namespace iqsm {

    struct FieldAbstract {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~FieldAbstract() = default;
    };
}
