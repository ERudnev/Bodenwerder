#pragma once

// Compatibility alias: persistency form lives in aspect::persistency (Meta::describe).

#include <fQSM/aspect/persistency.h>

namespace fqsm::processing::persistency::database {

    template<typename Meta>
    concept HasRetrospection = fqsm::aspect::HasRetrospection<Meta>;

}
