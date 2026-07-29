#pragma once

#include <tommy/gameObject.h>
#include <tommy/gun.h>

#include <rmmr/scene/camera.q1.h>

#include <fQSM/api/interface.h>

namespace tommy {

    using namespace fqsm::api;

    struct Player : Feature<Player, GameObject> {
        static constexpr integer sprite_index = 200; // Kenney playerShip1_blue
        static constexpr float sprite_scale = 0.5f;
        static constexpr float thrust_per_step = 0.005f;
        static constexpr float turn_degrees_per_step = 0.35f;
        static constexpr float hull_size = 1.0f;
        static constexpr integer max_hitpoints = 100;
        struct Quantum {
            rmmr::scene::Camera::Id camera;
            Custody<Gun> gun;
        };
        struct Actions : BaseActions {
            static void applyThrusters(Writing);
            static void tryFire(Writing);
            static void followCamera(Writing);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
