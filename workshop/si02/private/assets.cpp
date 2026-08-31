#include "assets.h"

#include <base/logging.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/sprites.q1.h>

namespace si02 {

    using namespace fqsm::api;
    using namespace rmmr;

    auto Assets::init(Writing context) -> std::unique_ptr<Assets> {
        using namespace resource;
        using geometry::Generator;

        base::message("workshop_si02: adding Kenney sprite pack...");
        const auto kenney = with<::rmmr::resource::Assets>::add_sprites_kenney(
            context,
            Unit::Name::from("workshop/si02", "space_shooter_kenney"),
            item<sprite::LoaderKenney>{
                .image = "sprites/Spritesheet/sheet.png",
                .descriptor = "sprites/Spritesheet/sheet.xml",
            });

        base::message("workshop_si02: adding unit quad geometry...");
        const auto unitQuad = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            Unit::Name::from("workshop/si02", "sprite_unit_quad"),
            item<Generator>{.type = Generator::Type::unitQuad, .subdivisions = 0});

        return std::unique_ptr<Assets>(new Assets{
            .kenney = kenney,
            .unitQuad = unitQuad,
        });
    }

}
