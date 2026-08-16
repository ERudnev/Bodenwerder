#pragma once

#include <eltanin/physics/atomic.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::geo {

    using namespace fqsm::api;

    using Mix = std::uint64_t;

    struct Volume {
        index3 origin;
        integer scale;
        Mix mix;
        vector<Volume> children;
    };

    struct Rock : Entity<Rock> {
        struct Quantum {
            Custody<phys::Atomic> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            Volume volume;
        };
        struct Actions : BaseActions {
            static auto spawn(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose, Volume) -> Id;
            static auto spawnIceSphere(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id, rmmr::Pose) -> Id;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
