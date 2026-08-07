#include "story.h"

#include <rmmr/api/_interface.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <si02/gameObject.h>
#include <si02/gun.h>
#include <si02/player.h>
#include <si02/shot.h>
#include <si02/stone.h>
#include <si02/sun.h>
#include <si02/world.h>

#include <algorithm>
#include <cmath>
#include <random>

namespace si02 {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr float k_screen_w = 1600.0f;
        constexpr float k_screen_h = 900.0f;
        constexpr float k_field_w = 5.0f * k_screen_w;
        constexpr float k_field_h = 5.0f * k_screen_h;

        // Circular-orbit speed for Hooke accel a = -pull * r (dt = 1 step): |vel| = r * sqrt(pull).
        auto circular_orbit_vel(float x, float y, float pull) -> vec3 {
            const float r = std::sqrt(x * x + y * y);
            if (r < 1.0f) {
                return vec3{0.0f, 0.0f, 0.0f};
            }
            const float speed = r * std::sqrt(pull);
            return vec3{-(y / r) * speed, (x / r) * speed, 0.0f};
        }

        auto spawn_stone(
            Writing context,
            scene::Root::Id root,
            resource::sprite::Pack::Id pack,
            resource::material::Asset::Id material,
            Pos pos,
            float bank,
            float size,
            RGB tint) -> GameObject::Id
        {
            const float scale = Stone::sprite_scale * size;
            const auto sprite = with<scene::Flat2d>::createSpriteActor(
                context,
                root,
                Pose::from(pos, HPB{0.0f, 0.0f, bank}),
                item<scene::actor::Sprite>{
                    .material = material,
                    .tint = tint,
                    .opacity = 1.0f,
                    .scale = vec3{scale, scale, scale},
                    .pack = pack,
                    .index = Stone::sprite_index,
                });
            const auto body = with<GameObject>::create(context, GameObject::Quantum{
                .sprite = sprite,
            });
            with<Physical>::extend(context, body, Physical::Quantum{
                .size = size,
                .mass = size,
                .hitpoints = std::max(
                    1,
                    static_cast<integer>(std::lround(
                        size * static_cast<float>(Player::max_hitpoints) / Player::hull_size))),
            });
            with<Stone>::extend(context, body, Stone::Quantum{});
            return body;
        }

        auto spawn_sun(
            Writing context,
            scene::Root::Id root,
            resource::sprite::Pack::Id pack,
            resource::material::Asset::Id material) -> GameObject::Id
        {
            const float size = Sun::hull_size;
            const float scale = Sun::sprite_scale * size;
            const auto sprite = with<scene::Flat2d>::createSpriteActor(
                context,
                root,
                Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}),
                item<scene::actor::Sprite>{
                    .material = material,
                    .tint = RGB{0.55f, 0.25f, -0.35f},
                    .opacity = 1.0f,
                    .scale = vec3{scale, scale, scale},
                    .pack = pack,
                    .index = Sun::sprite_index,
                });
            const auto body = with<GameObject>::create(context, GameObject::Quantum{
                .sprite = sprite,
            });
            with<Physical>::extend(context, body, Physical::Quantum{
                .size = size,
                .mass = size * 200.0f,
                .hitpoints = Sun::max_hitpoints,
            });
            with<Sun>::extend(context, body, Sun::Quantum{});
            return body;
        }

        auto spawn_player(
            Writing context,
            scene::Root::Id root,
            scene::Camera::Id camera,
            resource::sprite::Pack::Id pack,
            resource::material::Asset::Id material,
            Pos pos,
            vec3 vel) -> GameObject::Id
        {
            const float size = Player::hull_size;
            const float scale = Player::sprite_scale * size;
            const auto sprite = with<scene::Flat2d>::createSpriteActor(
                context,
                root,
                Pose::from(pos, HPB{0.0f, 0.0f, 0.0f}),
                item<scene::actor::Sprite>{
                    .material = material,
                    .tint = RGB{0.0f, 0.0f, 0.0f},
                    .opacity = 1.0f,
                    // Kenney ship art is nose-down; flip sprite only (Node/thrust stay +Y).
                    .scale = vec3{-scale, -scale, scale},
                    .pack = pack,
                    .index = Player::sprite_index,
                });
            const auto body = with<GameObject>::create(context, GameObject::Quantum{
                .sprite = sprite,
            });
            with<Physical>::extend(context, body, Physical::Quantum{
                .size = size,
                .mass = size,
                .hitpoints = Player::max_hitpoints,
            });
            with<Inertia>::extend(context, body, Inertia::Quantum{
                .vel = vel,
                .saturation = 0.0f,
            });
            base::maybe<World::Id> world_id;
            for (const auto entry : context->aspect<World>().items()) {
                world_id = entry.id;
                break;
            }
            if (not world_id) {
                return body;
            }
            const auto gun = with<Gun>::create(context, Gun::Quantum{
                .world = *world_id,
                .mech_ready_at = 0,
                .temperature_celsius = 0,
                .cool_step_carry = 0,
                .pending_shots = 0,
                .owner = {},
            });
            with<Player>::extend(context, body, Player::Quantum{
                .camera = camera,
                .gun = gun,
            });
            return body;
        }

    } // namespace

    Schema SpriteTest::schema() const {
        return ask::schema::merge({
            ask::schema::aspect<World>(),
            ask::schema::aspect<GameObject>(),
            ask::schema::aspect<AnimatedDecay>(),
            ask::schema::aspect<Physical>(),
            ask::schema::aspect<Inertia>(),
            ask::schema::aspect<Stone>(),
            ask::schema::aspect<Sun>(),
            ask::schema::aspect<Gun>(),
            ask::schema::aspect<Shot>(),
            ask::schema::aspect<Player>(),
        });
    }

    void SpriteTest::setup(Writing context, system::Window::Id window) {
        with<World>::create(context, World::Quantum{.step = 0, .paused = false});

        const auto framebuffer = with<system::Window>::framebufferSize(context, window);
        const auto viewport = with<system::Viewport_group>::addElement(context, window, system::Viewport::Quantum{
            .origin = index2{0, 0},
            .size = framebuffer,
            .clear_color = vec4{0.02f, 0.02f, 0.05f, 1.0f},
        });

        const auto root = with<scene::Interface>::createScene(context);
        with<scene::Flat2d>::extend(context, root, scene::Flat2d::Quantum{
            .size = index2{static_cast<integer>(k_screen_w), static_cast<integer>(k_screen_h)},
        });
        with<scene::actor::Sprite>::modify_global(context)->geometry = assets->unitQuad;

        const auto camera = with<scene::Flat2d>::createCamera(context, root,
            Pose::from(Pos{0.0f, 0.0f, 5.0f}, HPB{0.0f, 0.0f, 0.0f}));

        spawn_sun(context, root, assets->kenney, *shared->material.sprite);

        std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> x_dist{-0.5f * k_field_w, 0.5f * k_field_w};
        std::uniform_real_distribution<float> y_dist{-0.5f * k_field_h, 0.5f * k_field_h};
        std::uniform_real_distribution<float> bank_dist{0.0f, 360.0f};
        std::uniform_real_distribution<float> size_dist{0.5f, 2.0f};
        std::uniform_real_distribution<float> dark_dist{-0.22f, -0.08f};
        std::uniform_real_distribution<float> jitter{0.92f, 1.08f};

        constexpr float min_orbit_r = 220.0f;
        constexpr integer stone_count = 220;
        for (integer i = 0; i < stone_count; ++i) {
            float x = 0.0f;
            float y = 0.0f;
            float r = 0.0f;
            for (integer attempt = 0; attempt < 32; ++attempt) {
                x = x_dist(rng);
                y = y_dist(rng);
                r = std::sqrt(x * x + y * y);
                if (r >= min_orbit_r) {
                    break;
                }
            }
            if (r < min_orbit_r) {
                continue;
            }
            const float dark = dark_dist(rng);
            const float size = size_dist(rng);
            const auto body = spawn_stone(
                context,
                root,
                assets->kenney,
                *shared->material.sprite,
                Pos{x, y, 0.0f},
                bank_dist(rng),
                size,
                RGB{dark, dark, dark});
            auto vel = circular_orbit_vel(x, y, Sun::pull);
            vel *= jitter(rng);
            with<Inertia>::extend(context, body, Inertia::Quantum{
                .vel = vel,
                .saturation = 0.0f,
            });
        }

        constexpr float player_r = 700.0f;
        const Pos player_pos{0.0f, -player_r, 0.0f};
        spawn_player(
            context,
            root,
            camera,
            assets->kenney,
            *shared->material.sprite,
            player_pos,
            circular_orbit_vel(player_pos.x, player_pos.y, Sun::pull));
        with<Player>::followCamera(context);

        views = {
            View{.viewport = viewport, .scene = root, .camera = camera},
        };
    }

    void SpriteTest::onFrame(establish::Realm&, int64) {
        // Sim advances via World reactions on system::Clock (beginFrame).
    }

}
