#pragma once

#include <base/maybe.h>
#include <si02/gameObject.h>
#include <si02/world.h>

#include <fQSM/api/interface.h>

namespace si02 {

    using namespace fqsm::api;

    struct Gun : Entity<Gun> {
        static constexpr integer mech_cooldown_steps = 250;
        static constexpr integer temp_max_celsius = 500;
        static constexpr integer fire_below_celsius = 400;
        static constexpr integer heat_per_shot_celsius = 100;
        static constexpr integer cool_celsius_per_sec = 100;
        static constexpr float muzzle_lift = 40.0f;
        struct Quantum {
            Affected<World> world;
            integer mech_ready_at = 0;
            integer temperature_celsius = 0;
            integer cool_step_carry = 0;
            integer pending_shots = 0;
            base::maybe<GameObject::Id> owner;
        };
        struct Actions : BaseActions {
            static void requestFire(Writing, Id, GameObject::Id ship);
            static void flushPending(Writing);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
