#include "story.h"

#include <rmmr/api/_interface.h>
#include <rmmr/controller/camera2d.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <tommy/placeholder.h>

namespace tommy {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema SpriteTest::schema() const {
        return ask::schema::aspect<Placeholder>();
    }

    void SpriteTest::setup(Writing context, system::Core::Id, system::Viewport::Id viewport) {
        const auto root = with<scene::Interface>::createScene(context);

        with<scene::Flat2d>::extend(context, root, scene::Flat2d::Quantum{
            .size = index2{1600, 900},
        });
        with<scene::actor::Sprite>::modify_global(context)->geometry = assets->unitQuad;

        const auto spawn = [&](index2 pos, integer zet, integer index, float scale) {
            with<scene::Flat2d>::createSpriteActor(context, root,
                Locator{
                    .pos = Pos{
                        static_cast<float>(pos.x),
                        static_cast<float>(pos.y),
                        static_cast<float>(zet),
                    },
                    .euler = HPB{0.0f, 0.0f, 0.0f},
                },
                item<scene::actor::Sprite>{
                    .material = *shared->material.sprite,
                    .tint = RGB{0.0f, 0.0f, 0.0f},
                    .scale = vec3{scale, scale, 1.0f},
                    .pack = assets->kenney,
                    .index = index,
                });
        };

        constexpr integer grid = 10;
        constexpr integer step = 100;
        for (integer row = 0; row < grid; ++row) {
            for (integer col = 0; col < grid; ++col) {
                spawn(index2{col * step, row * step}, 0, row * grid + col, 1.0f);
            }
        }

        const auto camera = with<scene::Flat2d>::createCamera(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 5.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}});
        with<controller::Camera2d>::create(context, camera);

        views = {
            View{.viewport = viewport, .scene = root, .camera = camera},
        };
    }

}
