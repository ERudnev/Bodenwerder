#include <rmmr/resources/builders/materialPresets.h>

#include <rmmr/semantics/uniform.h>

namespace rmmr::resource::builders::material {

    using resource::material::Asset;

    namespace {

        auto inheritedDepth(renderer::BlendMode blend) -> renderer::RenderState {
            return renderer::RenderState{.blend = blend, .depthTest = renderer::ToggleMode::inherit, .depthWrite = renderer::ToggleMode::inherit, .depthCompare = renderer::DepthCompare::inherit};
        }

        auto shadowDepthTechnique(resource::shader::Reference program) -> Asset::Technique {
            return Asset::Technique{
                .program = program,
                .uniforms = {},
                .glowSpread = false,
                .lighting = renderer::LightingMode::unlit,
            };
        }

    } // namespace

    auto Presets::ambient(resource::shader::Reference program, resource::shader::Reference shadow_depth) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::opaque, Asset::Technique{
                    .program = program,
                    .uniforms = {},
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::unlit,
                }},
                {renderer::Pass::shadow, shadowDepthTechnique(shadow_depth)},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::inherit),
        };
    }

    auto Presets::lit(resource::shader::Reference program, resource::shader::Reference shadow_depth) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::opaque, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "shadowMap",
                    }),
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::primary,
                }},
                {renderer::Pass::shadow, shadowDepthTechnique(shadow_depth)},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::inherit),
        };
    }

    auto Presets::litTransparent(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::transparent, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "shadowMap",
                    }),
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::primary,
                }},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::alpha),
        };
    }

    auto Presets::litTextured(resource::shader::Reference program, resource::shader::Reference shadow_depth) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::opaque, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "albedoMap",
                        "shadowMap",
                    }),
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::primary,
                }},
                {renderer::Pass::shadow, shadowDepthTechnique(shadow_depth)},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::inherit),
        };
    }

    auto Presets::litTexturedTransparent(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::transparent, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "albedoMap",
                        "shadowMap",
                    }),
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::primary,
                }},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::inherit),
        };
    }

    auto Presets::oneSidedGlass(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::transparent, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "albedoMap",
                    }),
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::primary,
                }},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::alpha),
        };
    }

    auto Presets::gizmoTextured(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::gizmo, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "albedoMap",
                    }),
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::unlit,
                }},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::alpha),
        };
    }

    auto Presets::gizmoVertexColor(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::gizmo, Asset::Technique{
                    .program = program,
                    .uniforms = {},
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::unlit,
                }},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::alpha),
        };
    }

    auto Presets::unlit(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::gizmo, Asset::Technique{
                    .program = program,
                    .uniforms = {},
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::unlit,
                }},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::inherit),
        };
    }

    auto Presets::grid(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::transparent, Asset::Technique{
                    .program = program,
                    .uniforms = {},
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::unlit,
                }},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::inherit),
        };
    }

    auto Presets::sprite(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::sprite, Asset::Technique{
                    .program = program,
                    .uniforms = ::rmmr::material::Semantics::ids_of({
                        "atlasTexture",
                        "atlasEntries",
                        "inverseAtlasSize",
                    }),
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::unlit,
                }},
            },
            .nearest = true,
            .renderState = inheritedDepth(renderer::BlendMode::inherit),
        };
    }

    auto Presets::identity(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::identity, Asset::Technique{
                    .program = program,
                    .uniforms = {},
                    .glowSpread = false,
                    .lighting = renderer::LightingMode::unlit,
                }},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::inherit),
        };
    }

    auto Presets::familyGlow(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::opaque, Asset::Technique{
                    .program = program,
                    .uniforms = {},
                    .glowSpread = true,
                    .lighting = renderer::LightingMode::unlit,
                }},
            },
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::inherit),
        };
    }

}
