#include "locality/assets.h"

#include <rmmr/resources/builders/geometryGenerator.h>
#include <rmmr/resources/builders/materialBuilder.h>
#include <rmmr/resources/runtimes.q1.h>

#include <array>

namespace eltanin::locality::assets {

    using namespace rmmr;
    using rmmr::resource::geometry::Generator;
    using rmmr::resource::builders::material::SinglePass;

    namespace {

        struct EffectSpec {
            const char* name;
            const char* fragment;
            renderer::BlendMode blend;
            bool glowSpread;
        };

        constexpr std::array effectSpecs{
            EffectSpec{.name = "flash", .fragment = "shaders/flash.frag.glsl", .blend = renderer::BlendMode::additive, .glowSpread = true},
            EffectSpec{.name = "flashGlow", .fragment = "shaders/flashGlow.frag.glsl", .blend = renderer::BlendMode::additive, .glowSpread = true},
            EffectSpec{.name = "flashBrisance", .fragment = "shaders/flashBrisance.frag.glsl", .blend = renderer::BlendMode::alpha, .glowSpread = false},
            EffectSpec{.name = "dust", .fragment = "shaders/dust.frag.glsl", .blend = renderer::BlendMode::additive, .glowSpread = true},
        };

        auto renderState(renderer::BlendMode blend) -> renderer::RenderState {
            return renderer::RenderState{.blend = blend, .depthTest = renderer::ToggleMode::inherit, .depthWrite = renderer::ToggleMode::inherit, .depthCompare = renderer::DepthCompare::inherit};
        }

    }

    void add(Writing context) {
        using Assets = rmmr::resource::Assets;
        using Name = rmmr::resource::Unit::Name;
        for (const auto& spec : effectSpecs) {
            rmmr::resource::builders::material::addSinglePass(context, SinglePass{
                .name = Name::from("Eltanin", spec.name),
                .shader = item<rmmr::resource::shader::Loader>{.vertex = "shaders/flash.vert.glsl", .fragment = spec.fragment},
                .pass = renderer::Pass::transparent,
                .uniforms = {},
                .glowSpread = spec.glowSpread,
                .lighting = renderer::LightingMode::unlit,
                .nearest = false,
                .renderState = renderState(spec.blend),
            });
        }
        with<Assets>::add_geometry_generator(context, Name::from("Eltanin", "flashSphere"), item<Generator>{.type = Generator::Type::sphere, .subdivisions = 4});
    }

}
