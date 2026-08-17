#pragma once

#include <eltanin/physics/clast.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::geo {

    using namespace fqsm::api;

    struct Boulder : Entity<Boulder> {
        struct Recipe {
            integer mineral;
            float diameterMeters;
            float lump;
            integer seed;
        };
        struct Quantum {
            Custody<phys::Clast> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            integer mineral;
            float diameterMeters;
        };
        struct Actions : BaseActions {
            static auto spawn(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose, Recipe, vec3, vec3) -> Id;
            //@ *syncPose(~phys::Clast) — copy restored pose onto the Mesh node
            static void syncPose(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
