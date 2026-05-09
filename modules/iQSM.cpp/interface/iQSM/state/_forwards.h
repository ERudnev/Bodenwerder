#pragma once

#include <iQSM/typeId.h>
#include <iQSM/references.h>

namespace iqsm {
    namespace state {
        using RAId = internals::Types::RuntimeId;
    }
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

// TODO: consider to remove or clarify:
namespace iqsm::state::slice {
    struct Abstract;
}

// TODO: clarify this place as best to define this Axis...
namespace iqsm::state::policy {
    enum class versioning { // syntax: yep, small first character. enum class is not a type, it is namespace...
        shared,
        single,
    };

    enum class role {
        value,
        patch,
    };
} // policy