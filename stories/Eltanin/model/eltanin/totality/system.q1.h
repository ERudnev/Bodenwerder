#pragma once

#include <eltanin/totality/space.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::totality {

    using namespace fqsm::api;

    struct System : Entity<System> {
        struct Quantum {
            space::Pose pose;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
