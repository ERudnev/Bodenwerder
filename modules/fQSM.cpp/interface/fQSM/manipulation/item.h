#pragma once

#include <fQSM/identifier.h>
#include <fQSM/processing/transactions/quantal.h>
#include <fQSM/processing/contexts/operational.h>

// must disappear with cannonball refactoring and Writing ~ State& buffered writing
namespace fqsm::manipulation::item {
    template<category::Any Meta>
    using update = processing::transaction::Quantal<Meta>;
}
