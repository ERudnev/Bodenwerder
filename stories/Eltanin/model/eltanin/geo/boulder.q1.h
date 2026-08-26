#pragma once

#include <eltanin/geo/rock.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::geo {

    using namespace fqsm::api;

    struct Boulder : Entity<Boulder> {
        struct Quantum {
            Custody<phys::rigid::Ball> body;
            Custody<rmmr::scene::actor::Mesh> actor;
        };
        struct Actions : BaseActions {
            static auto spawnGenerated(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose, GeneralizedRecipe, vec3, vec3) -> Id;
            static void radiate(Stewarding, float dt);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
