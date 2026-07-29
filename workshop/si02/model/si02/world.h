#pragma once

#include <rmmr/system/core.q1.h>
#include <rmmr/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace si02 {

    using namespace fqsm::api;

    struct World : Entity<World> {
        struct Quantum {
            integer step = 0;
            bool paused = false;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
