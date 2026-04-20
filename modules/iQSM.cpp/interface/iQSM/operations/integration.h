#pragma once

#include <iQSM/_forwards.h>
#include <iQSM/repository/permit.h>
#include <iQSM/world.h>
#include <iQSM/delta.h>

namespace iqsm::operations {
    // Fast path: applies delta without validation (for Transaction and internal pipelines).
    World integrate(Reading, Delta);
    // Validate a snapshot against schema constraints (topological order / dependents).
    void validate_full(Writing);

    // Evaluate changed field-handles (pointers) and validate only affected types on `updated`.
    void validate_smart(Writing updated, Reading validBeforeChanges);

    Delta make_delta(Reading from, Reading to); // requires same schema handle
}