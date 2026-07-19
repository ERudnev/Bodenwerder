#pragma once

#include <fQSM/api/interface.h>

namespace toy {

    using namespace fqsm::api;

    struct God : Entity<God> {
        struct Quantum {
            integer time;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
