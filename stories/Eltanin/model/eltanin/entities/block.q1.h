#pragma once

#include <eltanin/physics/atomic.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin {

    using namespace fqsm::api;

    struct Block : Entity<Block> {
        struct Quantum {
            Custody<phys::Atomic> body;
            Custody<rmmr::scene::actor::Mesh> actor;
        };
        struct Actions : BaseActions {
            static auto spawn(Writing, rmmr::scene::Root::Id, rmmr::Locator, rmmr::scene::actor::Mesh::Quantum) -> Id;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
