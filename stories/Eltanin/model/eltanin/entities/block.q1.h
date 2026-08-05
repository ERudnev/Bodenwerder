#pragma once

#include <eltanin/physics/atomic.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>

#include <fQSM/api/interface.h>

#include "mech/semantics/together.include.h"

namespace eltanin {

    using namespace fqsm::api;

    struct Block : Entity<Block> {
        struct Quantum {
            Custody<phys::Atomic> body;
            Custody<rmmr::scene::actor::Mesh> actor;
        };
        struct Actions : BaseActions {
            static auto spawnPlate(Writing, rmmr::scene::Root::Id, rmmr::Pose, mech::plate::shape, mech::slot::plate, rmmr::scene::actor::Mesh::Quantum) -> Id;
            static auto spawnFrame(Writing, rmmr::scene::Root::Id, rmmr::Pose, mech::frame::shape, rmmr::scene::actor::Mesh::Quantum) -> Id;
            static auto spawnInner(Writing, rmmr::scene::Root::Id, rmmr::Pose, mech::inner::shape, mech::slot::inner, rmmr::scene::actor::Mesh::Quantum) -> Id;
            static auto spawnWing(Writing, rmmr::scene::Root::Id, rmmr::Pose, mech::wing::shape, mech::slot::wing, rmmr::scene::actor::Mesh::Quantum) -> Id;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
