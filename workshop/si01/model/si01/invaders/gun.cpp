#include <si01/invaders/gun.h>

#include <si01/invaders/combat.h>
#include <si01/invaders/visual.h>

#include <algorithm>

namespace si01::invaders {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        constexpr integer k_steps_per_sec = 1000; // World.step: 1 ms

        void applyCooling(Writing context, Gun::Id gun_id, integer steps) {
            if (steps <= 0 or not with<Gun>::exists(context, gun_id)) {
                return;
            }
            auto gun = with<Gun>::modify(context, gun_id);
            if (gun->temperature_celsius <= 0) {
                gun->temperature_celsius = 0;
                gun->cool_step_carry = 0;
                return;
            }
            // Accumulate: cool_celsius_per_sec degrees per k_steps_per_sec World steps.
            gun->cool_step_carry += steps * Gun::cool_celsius_per_sec;
            const integer drop = gun->cool_step_carry / k_steps_per_sec;
            gun->cool_step_carry %= k_steps_per_sec;
            if (drop > 0) {
                gun->temperature_celsius = std::max(integer{0}, gun->temperature_celsius - drop);
            }
        }

    } // namespace

    struct Gun::Internals : Gun::DefaultInternals {
        static void cool(Reacting context) {
            // Index Guns by World delta — Session is usually quiet on step ticks.
            const auto guns_by_world =
                ask::relations<World>(context).updated<Gun, &Gun::Quantum::world>();
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step or change.now.paused) {
                    continue;
                }
                const integer steps = change.now.step - change.old.step;
                for (const auto gun_id : guns_by_world.ids(change.id)) {
                    applyCooling(context, gun_id, steps);
                }
            }
        }
    };

    auto Gun::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Gun, World>(&Gun::Internals::cool),
        };
    }

    auto Gun::Actions::fire(
        Writing context,
        Id gun_id,
        Session::Id session_id,
        index2 muzzle) -> base::maybe<GameObject::Id>
    {
        if (not with<Gun>::exists(context, gun_id) or not with<Session>::exists(context, session_id)) {
            return {};
        }
        const auto world = with<Session>::get(context, session_id).world;
        const integer now_step = with<World>::get(context, world).step;
        auto gun = with<Gun>::modify(context, gun_id);
        if (now_step < gun->mech_ready_at) {
            return {};
        }
        if (gun->temperature_celsius >= Gun::fire_below_celsius) {
            return {};
        }
        const auto& session = with<Session>::get(context, session_id);
        const auto body = createGameObjectWithSprite(
            context,
            session,
            muzzle,
            Shot::sprite_index(Shot::Side::player),
            Shot::sprite_scale,
            Shot::sprite_bank,
            Shot::sprite_zet,
            rmmr::RGB{0.0f, 0.0f, 0.0f},
            Shot::max_hitpoints);
        with<Shot_group>::addElement(context, session_id, body, Shot::Quantum{
            .session = session_id,
            .pos = muzzle,
            .side = Shot::Side::player,
        });
        gun->mech_ready_at = now_step + Gun::mech_cooldown_steps;
        gun->temperature_celsius = std::min(
            Gun::temp_max_celsius,
            gun->temperature_celsius + Gun::heat_per_shot_celsius);
        return body;
    }

}
