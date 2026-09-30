#pragma once

#include <eltanin/cluster/space.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::cluster {

    using namespace fqsm::api;

    struct Axis : Entity<Axis> {
        struct Quantum {
            space::Pose pose;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Orbit : Feature<Orbit, Axis> {
        struct Quantum {
            Axis::Id parent;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    namespace doctrine {
        auto orbital() -> Schema;
    }

}
