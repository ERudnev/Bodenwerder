#pragma once

#include <si02/gameObject.h>

#include <fQSM/api/interface.h>

namespace si02 {

    using namespace fqsm::api;

    struct Shot : Feature<Shot, GameObject> {
        static constexpr integer sprite_index = 105; // Kenney laserBlue01
        static constexpr float sprite_scale = 2.5f;
        static constexpr float speed = 2.0f / 1.5f;
        static constexpr integer damage = 25;
        static constexpr float hull_size = 0.2f;
        static constexpr integer max_hitpoints = 1;
        static constexpr integer lifetime_steps = 2000;
        struct Quantum {
            integer expires_at = 0;
        };
        struct Actions : BaseActions {
            static void resolveHits(Writing);
            static void cullExpired(Writing);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
