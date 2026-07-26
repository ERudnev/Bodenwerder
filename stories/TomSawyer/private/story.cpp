#include "story.h"

#include <base/logging.h>
#include <rmmr/api/_interface.h>
#include <rmmr/controller/camera2d.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/sprites.q1.h>
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

    void SpriteTest::addAssets(Writing context, system::Core::Id core) {
        using namespace resource;
        using geometry::Generator;

        base::message("TomSawyer: seeding Kenney sprite pack...");
        assets.kenney = with<::rmmr::resource::Assets>::add_sprites_kenney(
            context,
            core,
            item<Unit>{.manager = core, .name = "space_shooter_kenney", .library = "TomSawyer"},
            item<sprite::LoaderKenney>{
                .image = "sprites/Spritesheet/sheet.png",
                .descriptor = "sprites/Spritesheet/sheet.xml",
            });

        base::message("TomSawyer: seeding unit quad geometry...");
        assets.unitQuad = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            core,
            item<Unit>{.manager = core, .name = "sprite_unit_quad", .library = "TomSawyer"},
            item<Generator>{.type = Generator::Type::unitQuad});
    }

    void SpriteTest::setup(Writing context, system::Core::Id, system::Viewport::Id viewport) {
        const auto root = with<scene::Interface>::createScene(context);

        with<scene::Flat2d>::extend(context, root, scene::Flat2d::Quantum{
            .size = index2{1600, 900},
        });
        with<scene::actor::Sprite>::modify_global(context)->geometry = *assets.unitQuad;

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
                    .tint = RGB{1.0f, 1.0f, 1.0f},
                    .scale = vec3{scale, scale, 1.0f},
                    .pack = *assets.kenney,
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
