#pragma once

#include <fQSM/typeId.h>
#include <fQSM/references.h>

namespace fqsm {
    
    using RAId = internals::Types::RuntimeId;

    namespace state {
        struct SchemaData;
    }
    // raise pointer alias to the root of iQSM:
    using Schema = fqsm::cref<state::SchemaData>;
}
