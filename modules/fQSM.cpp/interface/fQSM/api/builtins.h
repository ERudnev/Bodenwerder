#pragma once

#include <base/types/common_types.h>
#include <fQSM/identifier.h>

namespace fqsm::q1 {
    using namespace base::common_types;

    // Q1 link fields: anchor<T>, custody<T> are ids with lifecycle rules; affects<T> is a typed id only.
    template<typename Meta>
    using Anchor = typename Meta::Id;

    template<typename Meta>
    using Custody = typename Meta::Id;

    template<typename Meta>
    using Affected = ::fqsm::Affected<Meta>;
}
