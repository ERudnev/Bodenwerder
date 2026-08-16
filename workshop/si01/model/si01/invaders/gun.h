#pragma once

#include <base/maybe.h>
#include <si01/invaders/gameObject.h>
#include <si01/invaders/session.h>
#include <si01/world.h>

#include <fQSM/api/interface.h>

namespace si01::invaders {

    using namespace fqsm::api;

    // Doctrine: invaders/gun.q1 — aggregate under Player.gun custody.
    struct Gun : Entity<Gun> {
        static constexpr integer mech_cooldown_steps = 250;
        static constexpr integer temp_max_celsius = 500;
        static constexpr integer fire_below_celsius = 400;
        static constexpr integer heat_per_shot_celsius = 100;
        static constexpr integer cool_celsius_per_sec = 100;
        static constexpr integer muzzle_lift = 40;
        struct Quantum {
            Affected<World> world;
            integer mech_ready_at = 0;
            integer temperature_celsius = 0;
            integer cool_step_carry = 0;
        };
        struct Actions : BaseActions {
            // Player Shot GameObject id, or empty (mech gate / overheat).
            static auto fire(Writing, Id, Session::Id, index2 muzzle) -> base::maybe<GameObject::Id>;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
