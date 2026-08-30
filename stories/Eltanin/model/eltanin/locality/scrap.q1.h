#pragma once

#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    struct Scrap : Feature<Scrap, Thing> {
        struct Quantum {
            Custody<phys::rigid::Solid> body;
            Custody<rmmr::scene::actor::Mesh> actor;
        };
        struct Actions : BaseActions {
            static void update(Writing);
            static auto spawn(Writing, rmmr::Pose, vec3 halfExtents, float mass, vec3 linear, vec3 omega, float cohesion) -> Id;
            static void breakOff(Writing, vec3 worldCenter, quat worldRot, vec3 halfExtents, float mass, vec3 linear, float cohesion);
            static void followBody(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
