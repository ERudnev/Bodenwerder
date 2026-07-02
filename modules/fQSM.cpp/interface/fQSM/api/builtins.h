#pragma once

#include <base/types/common_types.h>
#include <fQSM/identifier.h>
#include <fQSM/aspect/assembly.interface.h>

namespace fqsm::q1 {
    using namespace base::common_types;

    template<typename Meta>
    using Anchor = ::fqsm::detail::aspect::Base::Anchor<Meta>;

    template<typename Meta>
    using Control = ::fqsm::detail::aspect::Base::Control<Meta>;
}
