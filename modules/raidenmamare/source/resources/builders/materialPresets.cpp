#include <rmmr/resources/builders/materialPresets.h>

#include <rmmr/semantics/uniform.h>

namespace rmmr::resource::builders::material {

    using resource::material::Asset;

    namespace {

        auto shadow_depth_technique(resource::shader::Reference program) -> Asset::Technique {
            return Asset::Technique{
                .program = program,
                .uniforms = {},
            };
        }

    } // namespace

    auto Presets::ambient(resource::shader::Reference program, resource::shader::Reference shadow_depth) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::opaque, Asset::Technique{
                    .program = program,
                    .uniforms = {},
                }},
                {renderer::Pass::shadow, shadow_depth_technique(shadow_depth)},
            },
            .nearest = false,
            .blend = renderer::BlendMode::inherit,
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
                }},
                {renderer::Pass::shadow, shadow_depth_technique(shadow_depth)},
            },
            .nearest = false,
            .blend = renderer::BlendMode::inherit,
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
                }},
            },
            .nearest = false,
            .blend = renderer::BlendMode::alpha,
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
                }},
                {renderer::Pass::shadow, shadow_depth_technique(shadow_depth)},
            },
            .nearest = false,
            .blend = renderer::BlendMode::inherit,
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
                }},
            },
            .nearest = false,
            .blend = renderer::BlendMode::inherit,
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
                }},
            },
            .nearest = false,
            .blend = renderer::BlendMode::alpha,
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
                }},
            },
            .nearest = false,
            .blend = renderer::BlendMode::alpha,
        };
    }

    auto Presets::gizmoVertexColor(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::gizmo, Asset::Technique{
                    .program = program,
                    .uniforms = {},
                }},
            },
            .nearest = false,
            .blend = renderer::BlendMode::alpha,
        };
    }

    auto Presets::grid(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::transparent, Asset::Technique{
                    .program = program,
                    .uniforms = {},
                }},
            },
            .nearest = false,
            .blend = renderer::BlendMode::inherit,
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
                }},
            },
            .nearest = true,
            .blend = renderer::BlendMode::inherit,
        };
    }

    auto Presets::identity(resource::shader::Reference program) -> Asset::Quantum {
        return Asset::Quantum{
            .techniques = {
                {renderer::Pass::identity, Asset::Technique{
                    .program = program,
                    .uniforms = {},
                }},
            },
            .nearest = false,
            .blend = renderer::BlendMode::inherit,
        };
    }

}
