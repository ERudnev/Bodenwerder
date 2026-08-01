#pragma once

#include <eltanin/physics/particle.q1.h>
#include <eltanin/resources/atomic.q1.h>
#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;

    struct Atomic : Entity<Atomic> {
        struct Quantum {
            vector<Particle::Id> particles;
            rmmr::Pose restored;
            resource::atomic::Asset::Id shape;
        };
        struct Actions : BaseActions {
            // Total impulse `imp` split evenly across particles (Verlet: kick via prev).
            static void debugAddImpulse(Writing, Id, vec3 imp);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
