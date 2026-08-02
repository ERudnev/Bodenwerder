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
            //@ *satisfy(~phys::Particle) — soft pin pass (Direct Nail + Particle)
            static void satisfy(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Gluon : Entity<Gluon> {
        struct Quantum {
            vector<Affected<Particle>> particles;
        };
        struct Actions : BaseActions {
            static auto clue(Writing, Particle::Id) -> Id;
            //@ *satisfy(~phys::Particle) — soft COM glue pass (Direct Gluon + Particle)
            static void satisfy(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
