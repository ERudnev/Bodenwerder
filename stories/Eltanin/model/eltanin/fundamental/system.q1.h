#pragma once

#include <eltanin/fundamental/space.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::fundamental {

    using namespace fqsm::api;

    struct System : Entity<System> {
        struct Quantum {
            space::Pose pose;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    namespace doctrine {
        auto system() -> Schema;
    }

}
