#pragma once

#include <base/maybe.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/family.q1.h>
#include <rmmr/scene/root.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    struct Bullet : Entity<Bullet> {
        struct Quantum {
            Custody<rmmr::scene::actor::Replica> actor;
            float speed;
        };
        struct Global {
            base::maybe<rmmr::scene::actor::Family::Id> shell30mm;
        };
        struct Actions : BaseActions {
            static void bind(Writing, rmmr::scene::Root::Id);
            static auto spawnShell30mm(Writing, rmmr::scene::Root::Id, rmmr::Pose, float speed) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions();
    };

}
