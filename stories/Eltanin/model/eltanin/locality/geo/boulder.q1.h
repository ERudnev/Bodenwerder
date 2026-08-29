#pragma once

#include <eltanin/locality/geo/rock.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::locality::geo {

    using namespace fqsm::api;

    struct Boulder : Feature<Boulder, Thing> {
        struct Quantum {
            Custody<phys::rigid::Solid> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            GeneralizedRecipe recipe;
        };
        struct Actions : BaseActions {
            static void update(Writing);
            static auto spawnGenerated(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose, GeneralizedRecipe, vec3, vec3) -> Id;
            static void radiate(Stewarding, float dt);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
