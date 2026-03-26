#pragma once

#include <iQSM/world.h>
#include <iQSM/delta.h>

namespace iqsm::ops {
    // Fast path: applies delta without validation (for Transaction and internal pipelines).
    World integrate(World, Delta);
    // Validate a snapshot against schema constraints (topological order / dependents).
    World validate_full(World);

    // Evaluate changed field-handles (pointers) and validate only affected types.
    // Requires worlds to share the same schema handle.
    World validate_smart(World validBeforeChanges, World changed);
    Delta make_delta(World from, World to); // requires same schema handle
}


