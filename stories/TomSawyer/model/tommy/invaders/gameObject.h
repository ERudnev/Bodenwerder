#pragma once

#include <base/maybe.h>
#include <rmmr/scene/actors/sprite.q1.h>

#include <fQSM/api/interface.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    // Doctrine: invaders/gameObject.q1.types — scene presence; role features share this Id.
    struct GameObject : Entity<GameObject> {
        struct Quantum {
            base::maybe<rmmr::scene::actor::Sprite::Id> sprite;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
