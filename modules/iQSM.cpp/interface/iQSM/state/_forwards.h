#pragma once

#include <iQSM/typeId.h>
#include <iQSM/references.h>

namespace iqsm {
    
    using RAId = internals::Types::RuntimeId;

    namespace state {
        struct SchemaData;
        struct DeltaData;
        struct WorldData;
    }
    // raise pointer alias to the root of iQSM:
    using Schema = iqsm::cref<state::SchemaData>;
    using Delta = iqsm::cref<state::DeltaData>;
    using World = iqsm::cref<state::WorldData>;
}
