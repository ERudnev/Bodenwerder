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
            static auto spawnHull(Writing, rmmr::scene::Root::Id, rmmr::Locator, mech::hull::shape, mech::slots::hull, rmmr::scene::actor::Mesh::Quantum) -> Id;
            static auto spawnFrame(Writing, rmmr::scene::Root::Id, rmmr::Locator, mech::frame::shape, mech::slots::frame, rmmr::scene::actor::Mesh::Quantum) -> Id;
            static auto spawnInner(Writing, rmmr::scene::Root::Id, rmmr::Locator, mech::inner::shape, mech::slots::inner, rmmr::scene::actor::Mesh::Quantum) -> Id;
            static auto spawnWing(Writing, rmmr::scene::Root::Id, rmmr::Locator, mech::wing::shape, mech::slots::wing, rmmr::scene::actor::Mesh::Quantum) -> Id;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
