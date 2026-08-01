#pragma once

#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;

    struct Particle : Entity<Particle> {
        struct Quantum {
            vec3 current{};
            vec3 prev{};
            float mass = 1.0f;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
