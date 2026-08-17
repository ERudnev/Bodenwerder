#pragma once

#include <eltanin/physics/particle.q1.h>
#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

#include <vector>

namespace eltanin::phys {

    using namespace fqsm::api;

    struct Clast : Entity<Clast> {
        struct Quantum {
            vector<Particle::Id> particles;
            float restRadius;
            rmmr::Pose restored;
        };
        struct Actions : BaseActions {
            //@ *satisfy(~Particle) — octahedron triad restore (Direct Clast + Particle)
            static void satisfy(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
