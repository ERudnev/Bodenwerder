#pragma once

#include <base/maybe.h>
#include <eltanin/locality/geo/rock.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::locality::geo {

    using namespace fqsm::api;

    struct Boulder : Feature<Boulder, Thing> {
        struct Resources {
            rmmr::resource::material::Asset::Id material;
            rmmr::resource::texture3array::Asset::Id crust;
        };
        struct Quantum {
            Custody<phys::rigid::Solid> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            GeneralizedRecipe recipe;
        };
        struct Global {
            base::maybe<Resources> resources;
        };
        struct Actions : BaseActions {
            static void bindResources(Writing);
            static void update(Writing);
            static auto spawnGenerated(Writing, rmmr::system::Device::Id, rmmr::Pose, GeneralizedRecipe, vec3, vec3) -> Id;
            static void radiate(Stewarding, seconds dt);
            static void followBody(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
