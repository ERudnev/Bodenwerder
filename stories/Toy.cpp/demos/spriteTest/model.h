#pragma once

#include <fQSM/api/interface.h>

namespace sprdemo {

    using namespace fqsm::api;

    // Placeholder product model entry — Product must contribute types to the app schema.
    struct God : Entity<God> {
        struct Quantum {
            integer time;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
