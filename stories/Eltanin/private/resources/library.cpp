#include "resources/library.h"

#include <eltanin/resources/geometry.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>
#include <rmmr/resources/builders/materialBuilder.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>

#include <array>

namespace eltanin::assets {

    using namespace rmmr;
    using rmmr::resource::geometry::Generator;
    using rmmr::resource::builders::material::Derived;
    using rmmr::resource::builders::material::SinglePass;

    namespace {

        auto inheritedDepth(renderer::BlendMode blend) -> renderer::RenderState {
            return renderer::RenderState{.blend = blend, .depthTest = renderer::ToggleMode::inherit, .depthWrite = renderer::ToggleMode::inherit, .depthCompare = renderer::DepthCompare::inherit};
        }

        struct PrimitiveSpec {
            base::maybe<rmmr::resource::geometry::Asset::Id> Primitives::* target;
            const char* name;
            Generator::Type type;
            integer subdivisions;
        };

        constexpr std::array primitiveSpecs{
            PrimitiveSpec{.target = &Primitives::grid, .name = "grid", .type = Generator::Type::gridPlane, .subdivisions = 0},
            PrimitiveSpec{.target = &Primitives::sphere, .name = "sphere", .type = Generator::Type::sphere, .subdivisions = 1},
            PrimitiveSpec{.target = &Primitives::kube, .name = "kube", .type = Generator::Type::kube, .subdivisions = 0},
            PrimitiveSpec{.target = &Primitives::diamond, .name = "diamond", .type = Generator::Type::diamond, .subdivisions = 0},
        };

    }

    auto addCore(Writing context, const rmmr::wrapper::assets::Handles& shared) -> base::maybe<Handles> {
        using Assets = rmmr::resource::Assets;
        using Name = rmmr::resource::Unit::Name;

        Handles handles{};
        for (const auto& spec : primitiveSpecs)
            handles.primitive.*spec.target = with<Assets>::add_geometry_generator(context, Name::from("Eltanin", spec.name), item<Generator>{.type = spec.type, .subdivisions = spec.subdivisions});

        handles.sprites = with<Assets>::add_texpack_catalog(context, Name::from("Eltanin", "sprites"), item<rmmr::resource::texpack::LoaderCatalog>{.directory = "sprites"}, index2{1024, 1024}, 8);
        handles.skySphereMaterial = rmmr::resource::builders::material::addSinglePass(context, SinglePass{
            .name = Name::from("Eltanin", "skySphere"),
            .shader = item<rmmr::resource::shader::Loader>{.vertex = "shaders/skySphere.vert.glsl", .fragment = "shaders/skySphere.frag.glsl"},
            .pass = renderer::Pass::environment,
            .uniforms = ::rmmr::material::Semantics::ids_of({"albedoMap"}),
            .glowSpread = true,
            .lighting = renderer::LightingMode::unlit,
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::additive),
        });
        handles.skyBackdropMaterial = rmmr::resource::builders::material::addSinglePass(context, SinglePass{
            .name = Name::from("Eltanin", "skyBackdrop"),
            .shader = item<rmmr::resource::shader::Loader>{.vertex = "shaders/skyBackdrop.vert.glsl", .fragment = "shaders/skyBackdrop.frag.glsl"},
            .pass = renderer::Pass::environment,
            .uniforms = {},
            .glowSpread = false,
            .lighting = renderer::LightingMode::unlit,
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::additive),
        });
        rmmr::resource::builders::material::addSinglePass(context, SinglePass{
            .name = Name::from("Eltanin", "skySun"),
            .shader = item<rmmr::resource::shader::Loader>{.vertex = "shaders/skyBackdrop.vert.glsl", .fragment = "shaders/skySun.frag.glsl"},
            .pass = renderer::Pass::environment,
            .uniforms = {},
            .glowSpread = true,
            .lighting = renderer::LightingMode::unlit,
            .nearest = false,
            .renderState = inheritedDepth(renderer::BlendMode::additive),
        });

        if (not shared.material.gizmo.textured)
            return context.refuse("eltanin::assets::addCore: rmmr gizmo_textured missing");
        handles.collisionDebugMaterial = rmmr::resource::builders::material::derive(context, Derived{
            .name = Name::from("Eltanin", "collisionDebug"),
            .source = *shared.material.gizmo.textured,
            .sourcePass = renderer::Pass::gizmo,
            .targetPass = renderer::Pass::opaque,
            .program = {},
            .glowSpread = false,
            .renderState = inheritedDepth(renderer::BlendMode::inherit),
        });
        if (not handles.collisionDebugMaterial)
            return {};

        const auto manager = with<rmmr::resource::Manager>::singleton(context);
        const auto skyGeometry = with<rmmr::resource::Unit_group>::addElement(context, manager, rmmr::resource::Unit::Quantum{.name = Name::from("Eltanin", "skySphere")});
        with<rmmr::resource::geometry::Asset>::extend(context, skyGeometry, rmmr::resource::geometry::Asset::Quantum{});
        with<resource::SkySphereGenerator>::extend(context, skyGeometry, resource::SkySphereGenerator::Quantum{.count = 48800, .seed = 1, .angular_diameter_deg = 0.41f});
        handles.skySphereGeometry = skyGeometry;

        const auto scrap = with<rmmr::resource::Unit_group>::addElement(context, manager, rmmr::resource::Unit::Quantum{.name = Name::from("Eltanin", "scrap")});
        with<rmmr::resource::geometry::Asset>::extend(context, scrap, rmmr::resource::geometry::Asset::Quantum{.entries = {}, .surfaces = {}, .mounts = {}, .entryCatalog = {}, .surfaceCatalogs = {}});
        handles.scrap = scrap;
        return handles;
    }

}
