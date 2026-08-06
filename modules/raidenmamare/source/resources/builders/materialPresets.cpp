#include <rmmr/resources/builders/materialPresets.h>

#include <rmmr/semantics/uniform.h>

namespace rmmr::resource::builders::material {

    using resource::material::Asset;

    namespace {

        auto shadow_depth_technique(resource::shader::Reference program) -> Asset::Technique {
            return Asset::Technique{
                .program = program,
                .uniforms = ::rmmr::material::Semantics::ids_of({
                    "model",
                    "lightSpaceMatrix",
                }),
                .textures = {},
            };
        }

    } // namespace

    auto Presets::ambient(resource::shader::Reference program, resource::shader::Reference shadow_depth) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::opaque, Asset::Technique{
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
                }},
                {renderer::Pass::shadow, shadow_depth_technique(shadow_depth)},
            },
        };
    }

    auto Presets::lit(resource::shader::Reference program, resource::shader::Reference shadow_depth) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::opaque, Asset::Technique{
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
                }},
                {renderer::Pass::shadow, shadow_depth_technique(shadow_depth)},
            },
        };
    }

    auto Presets::litTransparent(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::transparent, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "model",
                        "view",
                        "projection",
                        "albedo",
                        "opacity",
                        "ambientColor",
                        "ambientIntensity",
                        "light0Pos",
                        "light0Color",
                        "light0Intensity",
                        "lightSpaceMatrix",
                        "shadowMap",
                    }),
                    .textures = {},
                }},
            },
            .blend = renderer::BlendMode::alpha,
        };
    }

    auto Presets::litTextured(resource::shader::Reference program, resource::texture::Reference albedo_map, resource::shader::Reference shadow_depth) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::opaque, Asset::Technique{
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
                            .uniform = ::rmmr::material::Semantics::id_of("albedoMap"),
                            .texture = albedo_map,
                        },
                    },
                }},
                {renderer::Pass::shadow, shadow_depth_technique(shadow_depth)},
            },
        };
    }

    auto Presets::litTexturedTransparent(resource::shader::Reference program, resource::texture::Reference albedo_map) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::transparent, Asset::Technique{
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
                            .uniform = ::rmmr::material::Semantics::id_of("albedoMap"),
                            .texture = albedo_map,
                        },
                    },
                }},
            },
        };
    }

    auto Presets::oneSidedGlass(resource::shader::Reference program, resource::texture::Reference opacity_map) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::transparent, Asset::Technique{
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
                    }),
                    .textures = {
                        Asset::TextureBinding{
                            .uniform = ::rmmr::material::Semantics::id_of("albedoMap"),
                            .texture = opacity_map,
                        },
                    },
                }},
            },
            .blend = renderer::BlendMode::alpha,
        };
    }

    auto Presets::gizmoTextured(resource::shader::Reference program, resource::texture::Reference albedo_map) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::gizmo, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "model",
                        "view",
                        "projection",
                        "albedo",
                        "albedoMap",
                        "opacity",
                    }),
                    .textures = {
                        Asset::TextureBinding{
                            .uniform = ::rmmr::material::Semantics::id_of("albedoMap"),
                            .texture = albedo_map,
                        },
                    },
                }},
            },
            .blend = renderer::BlendMode::alpha,
        };
    }

    auto Presets::gizmoVertexColor(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::gizmo, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "model",
                        "view",
                        "projection",
                        "albedo",
                        "opacity",
                    }),
                    .textures = {},
                }},
            },
            .blend = renderer::BlendMode::alpha,
        };
    }

    auto Presets::grid(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::transparent, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "model",
                        "view",
                        "projection",
                        "patternScale",
                        "colorPrimary",
                        "colorSecondary",
                        "opacity",
                    }),
                    .textures = {},
                }},
            },
        };
    }

    auto Presets::sprite(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::sprite, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "model",
                        "view",
                        "projection",
                        "albedo",
                        "opacity",
                        "atlasTexture",
                        "atlasEntries",
                        "spriteIndex",
                        "inverseAtlasSize",
                    }),
                    .textures = {},
                }},
            },
            .nearest = true,
        };
    }

    auto Presets::identity(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::identity, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "model",
                        "view",
                        "projection",
                        "scenicAlias",
                    }),
                    .textures = {},
                }},
            },
        };
    }

}
