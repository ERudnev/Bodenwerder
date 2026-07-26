#pragma once

#include <fQSM/api/interface.h>

namespace tommy {

    using namespace fqsm::api;

    //@ in is not forward, it is mature Aspect definition (just minimal Aspect)
    struct Placeholder : Entity<Placeholder> {
        struct Quantum {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
