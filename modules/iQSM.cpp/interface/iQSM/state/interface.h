#pragma once

#include <iQSM/references.h>

namespace iqsm {
    namespace state {
        struct SchemaData;
        struct DeltaData;
    }
    // raise pointer alias to the root of iQSM:
    using Schema = iqsm::cref<state::SchemaData>;
    using Delta = iqsm::cref<state::DeltaData>;
}

// TODO: consider to remove or clarify:
namespace iqsm::state::slice {
    struct Abstract;
}