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

// TODO: clarify this place as best to define this Axis...
namespace iqsm::state::policy {
    enum class versioning { // syntax: yep, small first character. enum class is not a type, it is namespace...
        shared,
        single,
    };

    enum class order { // in math terms verlet integration as "state := (0 * state + 1 * patch)". I am not kidding!
        state,
        patch,
    };
} // policy