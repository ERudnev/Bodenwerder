#include "demos/spriteTest.h"

#include <base/logging.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/root.q1.h>

namespace toy::demos {

    using namespace fqsm::api;
    using namespace rmmr;

    void SpriteTest::seedAssets(Writing context, system::Core::Id core, const assets::Handles&) {
        using namespace resource;
        using geometry::Generator;

        base::message("spriteTest: seeding Kenney sprite pack...");
        assets.kenney = with<::rmmr::resource::Assets>::add_sprites_kenney(
            context,
            core,
            item<Unit>{.manager = core, .name = "space_shooter_kenney", .library = "Toy"},
            item<sprite::LoaderKenney>{
                .image = "sprites/Spritesheet/sheet.png",
                .descriptor = "sprites/Spritesheet/sheet.xml",
            });

        base::message("spriteTest: seeding unit quad geometry...");
        assets.unitQuad = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            core,
            item<Unit>{.manager = core, .name = "sprite_unit_quad", .library = "Toy"},
            item<Generator>{.type = Generator::Type::unitQuad});
    }

    auto SpriteTest::setup(Writing context, const assets::Handles&) -> Handles {
        const auto root = with<scene::Interface>::createScene(context);

        with<scene::Flat2d>::extend(context, root, scene::Flat2d::Quantum{
            .size = index2{1600, 900},
        });
        with<scene::actor::Sprite>::modify_global(context)->geometry = *assets.unitQuad;

        const auto camera = with<scene::Flat2d>::createCamera(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 5.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}});
        return Handles{.scene = root, .camera = camera};
    }

}
