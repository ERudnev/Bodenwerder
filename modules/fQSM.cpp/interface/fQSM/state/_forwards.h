#pragma once

#include <fQSM/meta/runtimeId.h>
#include <fQSM/references.h>

namespace fqsm {
    
    namespace state {
        struct SchemaData;

        namespace world {
            struct View;
            struct Patch;
        }
    }
    // raise pointer alias to the root of iQSM:
    using Schema = fqsm::cref<state::SchemaData>;
}
