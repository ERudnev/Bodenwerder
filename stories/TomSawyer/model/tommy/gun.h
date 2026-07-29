#pragma once

#include <base/maybe.h>
#include <tommy/gameObject.h>
#include <tommy/world.h>

#include <fQSM/api/interface.h>

namespace tommy {

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
        };
        struct Actions : BaseActions {
            static auto fire(Writing, Id, GameObject::Id ship) -> base::maybe<GameObject::Id>;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
