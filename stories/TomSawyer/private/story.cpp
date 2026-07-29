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

    Schema SpriteTest::schema() const {
        return ask::schema::merge({
            ask::schema::aspect<World>(),
            ask::schema::aspect<GameObject>(),
            ask::schema::aspect<Physical>(),
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
        std::uniform_real_distribution<float> x_dist{-700.0f, 700.0f};
        std::uniform_real_distribution<float> y_dist{-400.0f, 400.0f};
        std::uniform_real_distribution<float> bank_dist{0.0f, 360.0f};
        std::uniform_real_distribution<float> size_dist{0.5f, 2.0f};
        std::uniform_real_distribution<float> dark_dist{-0.22f, -0.08f};

        constexpr integer stone_count = 100;
        for (integer i = 0; i < stone_count; ++i) {
            const float x = x_dist(rng);
            const float y = y_dist(rng);
            const float bank = bank_dist(rng);
            const float size = size_dist(rng);
            const float scale = Stone::sprite_scale * size;
            const float dark = dark_dist(rng);
            const auto sprite = with<scene::Flat2d>::createSpriteActor(
                context,
                root,
                Locator{
                    .pos = Pos{x, y, 0.0f},
                    .euler = HPB{0.0f, 0.0f, bank},
                },
                item<scene::actor::Sprite>{
                    .material = *shared->material.sprite,
                    .tint = RGB{dark, dark, dark},
                    .scale = vec3{scale, scale, scale},
                    .pack = assets->kenney,
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
        }

        views = {
            View{.viewport = viewport, .scene = root, .camera = camera},
        };
    }

}
