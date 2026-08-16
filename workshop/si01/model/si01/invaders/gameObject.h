#pragma once

#include <fQSM/api/interface.h>
#include <rmmr/scene/actors/sprite.q1.h>

#include <base/maybe.h>

namespace si01::invaders {

    using namespace fqsm::api;

    // Doctrine: invaders/gameObject.q1 — scene presence + HP; role features share this Id.
    struct GameObject : Entity<GameObject> {
        struct Quantum {
            base::maybe<rmmr::scene::actor::Sprite::Id> sprite;
            integer hitpoints = 0;
        };
        struct Actions : BaseActions {
            // Remaining hitpoints after clamp (≥ 0).
            static auto takeDamage(Writing, Id, integer amount) -> integer;
            static auto alive(Reading, Id) -> bool;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
