#pragma once

#include <eltanin/physics/particle.q1.h>
#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys::strong {

    using namespace fqsm::api;

    struct Nail : Entity<Nail> {
        struct Quantum {
            Affected<Particle> particle;
            vec3 point;
        };
        struct Actions : BaseActions {
            static auto pin(Writing, Particle::Id) -> Id;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
