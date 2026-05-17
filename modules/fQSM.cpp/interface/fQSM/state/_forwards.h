#pragma once

#include <fQSM/typeId.h>
#include <fQSM/references.h>

namespace fqsm {
    
    using RAId = internals::Types::RuntimeId;

    namespace state {
        struct SchemaData;
        struct DeltaData;
        struct WorldData;
    }
    // raise pointer alias to the root of iQSM:
    using Schema = fqsm::cref<state::SchemaData>;
    using Delta = fqsm::cref<state::DeltaData>;
    using World = fqsm::cref<state::WorldData>;
}
