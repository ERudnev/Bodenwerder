#include "assets.h"

#include <base/logging.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/sprites.q1.h>

namespace si01 {

    using namespace fqsm::api;
    using namespace rmmr;

    auto Assets::init(Writing context, system::Core::Id core) -> std::unique_ptr<Assets> {
        using namespace resource;
        using geometry::Generator;

        base::message("workshop_si01: seeding Kenney sprite pack...");
        const auto kenney = with<::rmmr::resource::Assets>::add_sprites_kenney(
            context,
            core,
            item<Unit>{.manager = core, .name = "space_shooter_kenney", .library = "workshop/si01"},
            item<sprite::LoaderKenney>{
                .image = "sprites/Spritesheet/sheet.png",
                .descriptor = "sprites/Spritesheet/sheet.xml",
            });

        base::message("workshop_si01: seeding unit quad geometry...");
        const auto unitQuad = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            core,
            item<Unit>{.manager = core, .name = "sprite_unit_quad", .library = "workshop/si01"},
            item<Generator>{.type = Generator::Type::unitQuad});

        return std::unique_ptr<Assets>(new Assets{
            .kenney = kenney,
            .unitQuad = unitQuad,
        });
    }

}
