#pragma once

#include <base/maybe.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/family.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    struct Bullet : Feature<Bullet, Thing> {
        struct Quantum {
            Custody<rmmr::scene::actor::Replica> actor;
            Custody<phys::rigid::Ray> body;
            float speed;
        };
        struct Global {
            base::maybe<rmmr::scene::actor::Family::Id> shell30mm;
        };
        struct Actions : BaseActions {
            static void bind(Writing);
            static auto spawnShell30mm(Writing, rmmr::Pose, float speed) -> Id;
            static void update(Writing);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
