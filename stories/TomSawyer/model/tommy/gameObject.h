#pragma once

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>

namespace tommy {

    using namespace fqsm::api;

    struct GameObject : Entity<GameObject> {
        struct Quantum {
            base::maybe<rmmr::scene::actor::Sprite::Id> sprite;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

    struct Physical : Feature<Physical, GameObject> {
        struct Quantum {
            float size = 1.0f;
            float mass = 1.0f;
            integer hitpoints = 0;
        };
        struct Actions : BaseActions {
            static void resolveCollisions(Writing);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Inertia : Feature<Inertia, Physical> {
        struct Quantum {
            rmmr::vec3 vel{0.0f, 0.0f, 0.0f};
            float saturation = 0.0f;
        };
        struct Actions : BaseActions {
            static void update(Writing);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
