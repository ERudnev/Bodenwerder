#include <tommy/gun.h>

#include <tommy/player.h>
#include <tommy/shot.h>

#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <algorithm>
#include <cmath>

namespace tommy {

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
            gun->cool_step_carry += steps * Gun::cool_celsius_per_sec;
            const integer drop = gun->cool_step_carry / k_steps_per_sec;
            gun->cool_step_carry %= k_steps_per_sec;
            if (drop > 0) {
                gun->temperature_celsius = std::max(integer{0}, gun->temperature_celsius - drop);
            }
        }

        auto nose_xy(float bank_degrees) -> rmmr::vec2 {
            const float radians = glm::radians(bank_degrees);
            return rmmr::vec2{-std::sin(radians), std::cos(radians)};
        }

        auto first_scene_root(Reading context) -> base::maybe<rmmr::scene::Root::Id> {
            for (const auto entry : context->aspect<rmmr::scene::Root>().items()) {
                return entry.id;
            }
            return {};
        }

        auto spawn_bolt(Writing context, Gun::Id gun_id, GameObject::Id ship) -> base::maybe<GameObject::Id> {
            if (not with<Gun>::exists(context, gun_id) or not with<GameObject>::exists(context, ship)) {
                return {};
            }
            const auto world = with<Gun>::get(context, gun_id).world;
            if (not with<World>::exists(context, world)) {
                return {};
            }
            const integer now_step = with<World>::get(context, world).step;
            const auto& object = with<GameObject>::get(context, ship);
            if (not object.sprite) {
                return {};
            }
            if (not with<rmmr::scene::Node>::exists(context, *object.sprite)) {
                return {};
            }
            if (not with<rmmr::scene::actor::Sprite>::exists(context, *object.sprite)) {
                return {};
            }
            const auto root = first_scene_root(context);
            if (not root) {
                return {};
            }

            const auto& ship_node = with<rmmr::scene::Node>::get(context, *object.sprite);
            const auto& ship_sprite = with<rmmr::scene::actor::Sprite>::get(context, *object.sprite);
            const float bank = with<rmmr::scene::Node>::hpb(context, *object.sprite).z;
            const auto nose = nose_xy(bank);
            const rmmr::Pos muzzle{
                ship_node.position.x + nose.x * Gun::muzzle_lift,
                ship_node.position.y + nose.y * Gun::muzzle_lift,
                ship_node.position.z,
            };
            const float scale = Shot::sprite_scale * Shot::hull_size;
            const auto bolt_sprite = with<rmmr::scene::Flat2d>::createSpriteActor(
                context,
                *root,
                rmmr::Locator{
                    .pos = muzzle,
                    .euler = rmmr::HPB{0.0f, 0.0f, bank},
                },
                item<rmmr::scene::actor::Sprite>{
                    .material = ship_sprite.material,
                    .tint = rmmr::RGB{0.0f, 0.0f, 0.0f},
                    .opacity = 1.0f,
                    .scale = vec3{scale, scale, scale},
                    .pack = ship_sprite.pack,
                    .index = Shot::sprite_index,
                });
            const auto body = with<GameObject>::create(context, GameObject::Quantum{
                .sprite = bolt_sprite,
            });
            with<Physical>::extend(context, body, Physical::Quantum{
                .size = Shot::hull_size,
                .mass = Shot::hull_size,
                .hitpoints = Shot::max_hitpoints,
            });
            with<Inertia>::extend(context, body, Inertia::Quantum{
                .vel = rmmr::vec3{nose.x * Shot::speed, nose.y * Shot::speed, 0.0f},
                .saturation = 0.0f,
            });
            with<Shot>::extend(context, body, Shot::Quantum{
                .expires_at = now_step + Shot::lifetime_steps,
            });
            return body;
        }

    } // namespace

    struct Gun::Internals : Gun::DefaultInternals {
        static void cool(Reacting context) {
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

    void Gun::Actions::requestFire(Writing context, Id gun_id, GameObject::Id ship) {
        if (not with<Gun>::exists(context, gun_id) or not with<GameObject>::exists(context, ship)) {
            return;
        }
        if (not with<Player>::exists(context, ship)) {
            return;
        }
        const auto world = with<Gun>::get(context, gun_id).world;
        if (not with<World>::exists(context, world)) {
            return;
        }
        const integer now_step = with<World>::get(context, world).step;
        auto gun = with<Gun>::modify(context, gun_id);
        if (now_step < gun->mech_ready_at) {
            return;
        }
        if (gun->temperature_celsius >= Gun::fire_below_celsius) {
            return;
        }
        gun->owner = ship;
        gun->pending_shots += 1;
        gun->mech_ready_at = now_step + Gun::mech_cooldown_steps;
        gun->temperature_celsius = std::min(
            Gun::temp_max_celsius,
            gun->temperature_celsius + Gun::heat_per_shot_celsius);
    }

    void Gun::Actions::flushPending(Writing context) {
        for (const auto entry : context->aspect<Gun>().items()) {
            const auto gun_id = entry.id;
            auto gun = with<Gun>::modify(context, gun_id);
            if (gun->pending_shots <= 0 or not gun->owner) {
                continue;
            }
            const auto ship = *gun->owner;
            while (gun->pending_shots > 0) {
                gun->pending_shots -= 1;
                spawn_bolt(context, gun_id, ship);
            }
        }
    }

}
