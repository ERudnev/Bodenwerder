#pragma once

#include <eltanin/physics/particle.q1.h>
#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

#include <vector>

namespace eltanin::phys {

    using namespace fqsm::api;

    struct Atomic : Entity<Atomic> {
        // Rest geometry in body frame (meters), owned after spawn (Asset forgotten).
        // Masses live on Particle (Horn/COM read them live).
        struct Rest {
            vector<vec3> centered;
            vec3 com;
        };
        struct Quantum {
            vector<Particle::Id> particles;
            Rest rest;
            rmmr::Pose restored;
        };
        struct Actions : BaseActions {
            // Total impulse `imp` split evenly across particles (Verlet: kick via prev).
            static void debugAddImpulse(Writing, Id, vec3 imp);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
