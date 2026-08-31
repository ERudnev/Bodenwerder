#pragma once

#include <base/maybe.h>
#include <eltanin/locality/geo/minerals.q1.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::locality::geo {

    using namespace fqsm::api;

    using Mix = std::uint64_t;

    struct Volume {
        index3 origin;
        integer scale;
        Mix mix;
        vector<Volume> children;
    };

    struct GeneralizedRecipe {
        Mix mix;
        float radius;
        float lump;
        integer seed;
        float spotMeters;
        float spotContrast;

        static auto homogenous(Mineral::Index) -> Mix;
    };

    struct Rock : Feature<Rock, Thing> {
        struct Resources {
            rmmr::resource::texture3array::Asset::Id crust;
        };
        struct Quantum {
            Custody<phys::rigid::Crystal> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            Volume volume;
        };
        struct Global {
            base::maybe<Resources> resources;
        };
        struct Actions : BaseActions {
            static void bindResources(Writing);
            static void update(Writing);
            static auto spawn(Writing, rmmr::system::Device::Id, rmmr::Pose, Volume, vec3, vec3) -> Id;
            static auto spawnGenerated(Writing, rmmr::system::Device::Id, rmmr::Pose, GeneralizedRecipe, vec3, vec3) -> Id;
            static auto spawnIceSphere(Writing, rmmr::system::Device::Id, rmmr::Pose) -> Id;
            static auto spawnPaletteTorus(Writing, rmmr::system::Device::Id, rmmr::Pose) -> Id;
            static auto spawnLavaBrick(Writing, rmmr::system::Device::Id, rmmr::Pose) -> Id;
            static auto spawnIceBlob(Writing, rmmr::system::Device::Id, rmmr::Pose) -> Id;
            static void radiate(Stewarding, seconds dt);
            static void followBody(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
