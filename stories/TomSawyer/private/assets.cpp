#include "assets.h"

#include <base/logging.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/sprites.q1.h>

namespace tommy {

    using namespace fqsm::api;
    using namespace rmmr;

    auto Assets::init(Writing context) -> std::unique_ptr<Assets> {
        using namespace resource;
        using geometry::Generator;

        base::message("TomSawyer: adding Kenney sprite pack...");
        const auto kenney = with<::rmmr::resource::Assets>::add_sprites_kenney(
            context,
            Unit::name("TomSawyer", "space_shooter_kenney"),
            item<sprite::LoaderKenney>{
                .image = "sprites/Spritesheet/sheet.png",
                .descriptor = "sprites/Spritesheet/sheet.xml",
            });

        base::message("TomSawyer: adding unit quad geometry...");
        const auto unitQuad = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            Unit::name("TomSawyer", "sprite_unit_quad"),
            item<Generator>{.type = Generator::Type::unitQuad});

        return std::unique_ptr<Assets>(new Assets{
            .kenney = kenney,
            .unitQuad = unitQuad,
        });
    }

}
