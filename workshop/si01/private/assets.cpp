#include "assets.h"

#include <base/logging.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/sprites.q1.h>

namespace si01 {

    using namespace fqsm::api;
    using namespace rmmr;

    auto Assets::init(Writing context) -> std::unique_ptr<Assets> {
        using namespace resource;
        using geometry::Generator;

        base::message("workshop_si01: adding Kenney sprite pack...");
        const auto kenney = with<::rmmr::resource::Assets>::add_sprites_kenney(
            context,
            item<Unit>{.name = "space_shooter_kenney", .library = "workshop/si01"},
            item<sprite::LoaderKenney>{
                .image = "sprites/Spritesheet/sheet.png",
                .descriptor = "sprites/Spritesheet/sheet.xml",
            });

        base::message("workshop_si01: adding unit quad geometry...");
        const auto unitQuad = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            item<Unit>{.name = "sprite_unit_quad", .library = "workshop/si01"},
            item<Generator>{.type = Generator::Type::unitQuad});

        return std::unique_ptr<Assets>(new Assets{
            .kenney = kenney,
            .unitQuad = unitQuad,
        });
    }

}
