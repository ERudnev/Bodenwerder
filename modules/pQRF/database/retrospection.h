#pragma once

// Compatibility alias: canonical check is meta::category::musthave::Retrospection.

#include <fQSM/meta/categories.h>

namespace fqsm::processing::persistency::database {

    template<typename Meta>
    concept HasRetrospection = fqsm::meta::category::musthave::Retrospection<Meta>;

}
