#pragma once

#include <fQSM/api/interface.h>

namespace kubes {

    using namespace fqsm::api;

    // Placeholder product model entry — Product must contribute types to the app schema.
    struct World : Entity<World> {
        struct Quantum {
            integer tick;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
