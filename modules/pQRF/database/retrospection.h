#pragma once

// Canonical check: meta::category::musthave::Retrospection (Retrospection<Meta>::describe).

#include <fQSM/meta/categories.h>

namespace fqsm::processing::persistency::database {

    template<typename Meta>
    concept HasRetrospection = fqsm::meta::category::musthave::Retrospection<Meta>;

}
