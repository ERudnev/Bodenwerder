#pragma once

#include <fQSM/meta/runtimeId.h>
#include <fQSM/references.h>


namespace fqsm::state {
    struct SchemaData;
}

namespace fqsm {
    // raise pointer alias to the root of iQSM:
    using Schema = fqsm::cref<state::SchemaData>;
}

namespace fqsm::state::world {
    struct View;
    struct Data;
    struct Patch;
}

