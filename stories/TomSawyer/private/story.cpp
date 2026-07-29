#include "story.h"

#include <rmmr/api/_interface.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <tommy/gameObject.h>
#include <tommy/stone.h>
#include <tommy/world.h>

#include <random>

namespace tommy {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

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
                Locator{
                    .pos = pos,
                    .euler = HPB{0.0f, 0.0f, bank},
                },
                item<scene::actor::Sprite>{
                    .material = material,
                    .tint = tint,
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
            });
            with<Stone>::extend(context, body, Stone::Quantum{});
            return body;
        }

    } // namespace

    Schema SpriteTest::schema() const {
        return ask::schema::merge({
            ask::schema::aspect<World>(),
            ask::schema::aspect<GameObject>(),
            ask::schema::aspect<Physical>(),
            ask::schema::aspect<Inertia>(),
            ask::schema::aspect<Stone>(),
        });
    }

    void SpriteTest::setup(Writing context, system::Core::Id, system::Viewport::Id viewport) {
        with<World>::create(context, World::Quantum{.step = 0, .paused = false});

        const auto root = with<scene::Interface>::createScene(context);
        with<scene::Flat2d>::extend(context, root, scene::Flat2d::Quantum{
            .size = index2{1600, 900},
        });
        with<scene::actor::Sprite>::modify_global(context)->geometry = assets->unitQuad;

        const auto camera = with<scene::Flat2d>::createCamera(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 5.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}});

        std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> x_dist{-500.0f, 700.0f};
        std::uniform_real_distribution<float> y_dist{-400.0f, 400.0f};
        std::uniform_real_distribution<float> bank_dist{0.0f, 360.0f};
        std::uniform_real_distribution<float> size_dist{0.5f, 2.0f};
        std::uniform_real_distribution<float> dark_dist{-0.22f, -0.08f};

        constexpr integer stone_count = 100;
        for (integer i = 0; i < stone_count; ++i) {
            const float dark = dark_dist(rng);
            spawn_stone(
                context,
                root,
                assets->kenney,
                *shared->material.sprite,
                Pos{x_dist(rng), y_dist(rng), 0.0f},
                bank_dist(rng),
                size_dist(rng),
                RGB{dark, dark, dark});
        }

        // Optional Inertia only on the rammer — field stones stay Physical-only.
        // vel is per World.step (≈1 ms); keep it modest so the pass is visible.
        const auto rammer = spawn_stone(
            context,
            root,
            assets->kenney,
            *shared->material.sprite,
            Pos{-650.0f, 0.0f, 0.0f},
            0.0f,
            4.0f,
            RGB{0.55f, -0.15f, -0.25f});
        with<Inertia>::extend(context, rammer, Inertia::Quantum{
            .vel = vec3{1.2f, 0.0f, 0.0f},
            .saturation = 0.0005f,
        });

        views = {
            View{.viewport = viewport, .scene = root, .camera = camera},
        };
    }

}
