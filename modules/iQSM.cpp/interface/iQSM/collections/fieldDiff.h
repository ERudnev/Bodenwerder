#pragma once

#include <iQSM/types.h>

namespace iqsm::delta {

    struct FieldDiffAbstract {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~FieldDiffAbstract() = default;
    };
}
