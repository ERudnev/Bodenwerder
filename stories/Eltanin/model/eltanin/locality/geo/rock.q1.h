#pragma once

#include <eltanin/locality/geo/minerals.q1.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>
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
        struct Quantum {
            Custody<phys::rigid::Crystal> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            Volume volume;
        };
        struct Actions : BaseActions {
            static void update(Writing);
            static auto spawn(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose, Volume, vec3, vec3) -> Id;
            static auto spawnGenerated(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose, GeneralizedRecipe, vec3, vec3) -> Id;
            static auto spawnIceSphere(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose) -> Id;
            static auto spawnPaletteTorus(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose) -> Id;
            static auto spawnLavaBrick(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose) -> Id;
            static auto spawnIceBlob(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose) -> Id;
            static void radiate(Stewarding, float dt);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
