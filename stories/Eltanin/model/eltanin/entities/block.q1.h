#pragma once

#include <eltanin/physics/atomic.q1.h>
#include <eltanin/resources/atomic.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/simple.q1.h>
#include <rmmr/scene/root.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin {

    using namespace fqsm::api;

    struct Block : Entity<Block> {
        struct Quantum {
            Custody<phys::Atomic> body;
            Custody<rmmr::scene::actor::Simple> actor;
        };
        struct Actions : BaseActions {
            static auto spawn(Writing, rmmr::scene::Root::Id, resource::atomic::Asset::Id, rmmr::Locator, rmmr::scene::actor::Simple::Quantum) -> Id;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
