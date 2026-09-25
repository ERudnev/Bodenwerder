#include "geo/assets.h"

#include <rmmr/resources/builders/geometryGenerator.h>
#include <rmmr/resources/builders/materialBuilder.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/resources/texture3array.q1.h>

#include <array>
#include <utility>

namespace eltanin::geo::assets {

    using namespace rmmr;
    using rmmr::resource::geometry::Generator;
    using rmmr::resource::builders::material::SinglePass;

    namespace {

        auto inheritedState(renderer::BlendMode blend) -> renderer::RenderState {
            return renderer::RenderState{.blend = blend, .depthTest = renderer::ToggleMode::inherit, .depthWrite = renderer::ToggleMode::inherit, .depthCompare = renderer::DepthCompare::inherit};
        }

        struct SurfaceSpec {
            const char* name;
            const char* vertex;
            const char* fragment;
            rmmr::resource::Uniform::Palette uniforms;
            bool glowSpread;
            bool shadow;
        };

        struct AtmosphereSpec {
            const char* name;
            const char* vertex;
            const char* fragment;
            rmmr::resource::Uniform::Palette uniforms;
        };

    }

    auto add(Writing context, const rmmr::wrapper::assets::Handles& shared) -> bool {
        using Assets = rmmr::resource::Assets;
        using Material = rmmr::resource::material::Asset;
        using Name = rmmr::resource::Unit::Name;

        if (not shared.material.lit) {
            context.refuse("eltanin::geo::assets::add: rmmr lit missing");
            return false;
        }
        const auto& lit = with<Material>::get(context, *shared.material.lit);
        const auto shadow = lit.techniques.find(renderer::Pass::shadow);
        if (shadow == lit.techniques.end()) {
            context.refuse("eltanin::geo::assets::add: lit shadow technique missing");
            return false;
        }

        const std::array surfaces{
            SurfaceSpec{.name = "rock", .vertex = "shaders/rock.vert.glsl", .fragment = "shaders/rock.frag.glsl", .uniforms = ::rmmr::material::Semantics::ids_of({"shadowMap", "minerals"}), .glowSpread = true, .shadow = true},
            SurfaceSpec{.name = "boulder", .vertex = "shaders/boulder.vert.glsl", .fragment = "shaders/boulder.frag.glsl", .uniforms = ::rmmr::material::Semantics::ids_of({"shadowMap", "minerals"}), .glowSpread = true, .shadow = true},
            SurfaceSpec{.name = "planet", .vertex = "shaders/planet.vert.glsl", .fragment = "shaders/planet.frag.glsl", .uniforms = ::rmmr::material::Semantics::ids_of({"shadowMap", "albedoMap", "heightMap", "coverMap", "farAlbedoMap", "farNormalMap"}), .glowSpread = false, .shadow = false},
        };
        for (const auto& spec : surfaces) {
            const auto shader = with<Assets>::add_shader_loader(context, Name::from("Eltanin", spec.name), item<rmmr::resource::shader::Loader>{.vertex = spec.vertex, .fragment = spec.fragment});
            auto techniques = umap<renderer::Pass, Material::Technique>{
                {renderer::Pass::opaque, Material::Technique{.program = with<rmmr::resource::Unit>::remember(context, shader), .uniforms = spec.uniforms, .glowSpread = spec.glowSpread, .lighting = renderer::LightingMode::primary}},
            };
            if (spec.shadow)
                techniques.emplace(renderer::Pass::shadow, shadow->second);
            with<Assets>::add_material(context, Name::from("Eltanin", spec.name), Material::Quantum{.techniques = std::move(techniques), .nearest = false, .renderState = inheritedState(renderer::BlendMode::inherit)});
        }

        const std::array atmospheres{
            AtmosphereSpec{.name = "atmosphere", .vertex = "shaders/atmosphere.vert.glsl", .fragment = "shaders/atmosphere.frag.glsl", .uniforms = ::rmmr::material::Semantics::ids_of({"sceneDepth"})},
            AtmosphereSpec{.name = "cloud", .vertex = "shaders/cloud.vert.glsl", .fragment = "shaders/cloud.frag.glsl", .uniforms = ::rmmr::material::Semantics::ids_of({"sceneDepth", "heightMap"})},
        };
        for (const auto& spec : atmospheres) {
            rmmr::resource::builders::material::addSinglePass(context, SinglePass{
                .name = Name::from("Eltanin", spec.name),
                .shader = item<rmmr::resource::shader::Loader>{.vertex = spec.vertex, .fragment = spec.fragment},
                .pass = renderer::Pass::atmosphere,
                .uniforms = spec.uniforms,
                .glowSpread = true,
                .lighting = renderer::LightingMode::primary,
                .nearest = false,
                .renderState = inheritedState(renderer::BlendMode::premultiplied),
            });
        }

        with<Assets>::add_geometry_generator(context, Name::from("Eltanin", "atmosphereSphere"), item<Generator>{.type = Generator::Type::sphere, .subdivisions = 4});
        with<Assets>::add_geometry_generator(context, Name::from("Eltanin", "patchGrid"), item<Generator>{.type = Generator::Type::patchGrid, .subdivisions = 32});
        const auto manager = with<rmmr::resource::Manager>::singleton(context);
        const auto crust = with<rmmr::resource::Unit_group>::addElement(context, manager, rmmr::resource::Unit::Quantum{.name = Name::from("Eltanin", "crust")});
        with<rmmr::resource::texture3array::Asset>::extend(context, crust, rmmr::resource::texture3array::Asset::Quantum{.layerSize = index3{0, 0, 0}, .capacity = 0});
        const auto facies = with<rmmr::resource::Unit_group>::addElement(context, manager, rmmr::resource::Unit::Quantum{.name = Name::from("Eltanin", "facies")});
        with<rmmr::resource::texpack::Pack>::extend(context, facies, rmmr::resource::texpack::Pack::Quantum{.layerSize = index2{1024, 1024}, .capacity = 48, .layers = {}, .compressed = true, .grayscale = false});
        with<rmmr::resource::texpack::LoaderCatalog>::extend(context, facies, rmmr::resource::texpack::LoaderCatalog::Quantum{.directory = "textures/facies"});
        return true;
    }

}
