#pragma once

#include <fQSM/api/interface.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    struct Thing : Entity<Thing> {
        struct Quantum {
            int64 bornAt;
        };
        struct Global {
            int64 now;
        };
        struct Actions : BaseActions {
            static void update(Writing, int64 dtUs);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
