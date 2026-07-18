#include <rmmr/resources/builders/materialPresets.h>

#include <rmmr/semantics/uniform.h>

namespace rmmr::resource::builders::material {

    using resource::material::Asset;

    auto MaterialPresets::ambient(resource::shader::Asset::Id program) -> Asset::Quantum {
        return Asset::Quantum{
            .program = program,
            .uniforms = ::rmmr::material::Semantics::ids_of({
                "model",
                "view",
                "projection",
                "albedo",
                "ambientColor",
                "ambientIntensity",
            }),
            .textures = {},
            .passes = renderer::PassesPresets::opaque_casting,
        };
    }

    auto MaterialPresets::lit(resource::shader::Asset::Id program) -> Asset::Quantum {
        return Asset::Quantum{
            .program = program,
            .uniforms = ::rmmr::material::Semantics::ids_of({
                "model",
                "view",
                "projection",
                "albedo",
                "ambientColor",
                "ambientIntensity",
                "light0Pos",
                "light0Color",
                "light0Intensity",
                "lightSpaceMatrix",
                "shadowMap",
            }),
            .textures = {},
            .passes = renderer::PassesPresets::opaque_casting,
        };
    }

    auto MaterialPresets::litTextured(resource::shader::Asset::Id program, resource::texture::Asset::Id albedo_map) -> Asset::Quantum {
        return Asset::Quantum{
            .program = program,
            .uniforms = ::rmmr::material::Semantics::ids_of({
                "model",
                "view",
                "projection",
                "albedo",
                "albedoMap",
                "ambientColor",
                "ambientIntensity",
                "light0Pos",
                "light0Color",
                "light0Intensity",
                "lightSpaceMatrix",
                "shadowMap",
            }),
            .textures = {
                Asset::TextureBinding{
                    .id = ::rmmr::material::Semantics::id_of("albedoMap"),
                    .texture = albedo_map,
                },
            },
            .passes = renderer::PassesPresets::opaque_casting,
        };
    }

    auto MaterialPresets::litTexturedTransparent(resource::shader::Asset::Id program, resource::texture::Asset::Id albedo_map) -> Asset::Quantum {
        auto quantum = litTextured(program, albedo_map);
        quantum.passes = renderer::PassesPresets::transparent_only;
        return quantum;
    }

    auto MaterialPresets::grid(resource::shader::Asset::Id program) -> Asset::Quantum {
        return Asset::Quantum{
            .program = program,
            .uniforms = ::rmmr::material::Semantics::ids_of({
                "model",
                "view",
                "projection",
                "patternScale",
                "colorPrimary",
                "colorSecondary",
            }),
            .textures = {},
            .passes = renderer::PassesPresets::opaque_only,
        };
    }

    auto MaterialPresets::shadowDepth(resource::shader::Asset::Id program) -> Asset::Quantum {
        return Asset::Quantum{
            .program = program,
            .uniforms = ::rmmr::material::Semantics::ids_of({
                "model",
                "lightSpaceMatrix",
            }),
            .textures = {},
            .passes = renderer::PassesPresets::shadow_only,
        };
    }

}
