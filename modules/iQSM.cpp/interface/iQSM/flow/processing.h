#pragma once

#include <iQSM/flow/_forwards.h>

namespace iqsm::flow {
    World integrate(Reading, Delta);
    void validateFull(Writing);
    // Evaluate changed field-handles (pointers) and validate only affected types on `updated`.
    void validateSmart(Writing updated, Reading lastValidState);
    Delta makeDelta(Reading from, Reading to); // requires same schema handle

}