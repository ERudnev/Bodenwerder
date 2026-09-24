#include "views/blueprints/editor.h"

#include <rmmr/resources/builders/materialBuilder.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/texpack.q1.h>

#include <array>

namespace eltanin::views {

    using namespace rmmr;
    using rmmr::resource::builders::material::Derived;

    namespace {

        auto inheritedDepth(renderer::BlendMode blend) -> renderer::RenderState {
            return renderer::RenderState{.blend = blend, .depthTest = renderer::ToggleMode::inherit, .depthWrite = renderer::ToggleMode::inherit, .depthCompare = renderer::DepthCompare::inherit};
        }

        struct MeshpackSpec {
            const char* name;
            const char* file;
        };

        constexpr std::array meshpackSpecs{
            MeshpackSpec{.name = "editor/interframe", .file = "Eltanin/meshes/editor/interframe.lwo.meshpack"},
            MeshpackSpec{.name = "editor/attachments", .file = "Eltanin/meshes/editor/attachments.lwo.meshpack"},
            MeshpackSpec{.name = "fittings/mounts/armour", .file = "Eltanin/meshes/fittings/mounts/armour.lwo.meshpack"},
            MeshpackSpec{.name = "fittings/devices/cannon_temp_solid", .file = "Eltanin/meshes/fittings/devices/cannon_temp_solid.lwo.meshpack"},
            MeshpackSpec{.name = "fittings/devices/controlRoomSmall", .file = "Eltanin/meshes/fittings/devices/controlRoomSmall.lwo.meshpack"},
            MeshpackSpec{.name = "misc/projectiles", .file = "Eltanin/meshes/misc/projectiles.lwo.meshpack"},
        };

    }

    auto Blueprints::addAssets(Writing context, const rmmr::wrapper::assets::Handles& shared) -> bool {
        using Assets = rmmr::resource::Assets;
        using Material = rmmr::resource::material::Asset;
        using Name = rmmr::resource::Unit::Name;

        const auto effectShader = with<Assets>::add_shader_loader(context, Name::from("Eltanin", "blueprintsEditorEffect"), item<rmmr::resource::shader::Loader>{.vertex = "shaders/blueprintsEditorEffect.vert.glsl", .fragment = "shaders/blueprintsEditorEffect.frag.glsl"});
        assets.editorEffect = with<Assets>::add_overlay(context, Name::from("Eltanin", "blueprintsEditorEffect"), rmmr::resource::overlay::Asset::Quantum{
            .program = with<rmmr::resource::Unit>::remember(context, effectShader),
            .uniforms = ::rmmr::material::Semantics::ids_of({"identiffyMap", "selectedMap", "under"}),
            .scale = rmmr::resource::overlay::Scale::full,
        });

        if (not shared.material.litTextured or not shared.material.litTexturedAlpha or not shared.material.litTransparent or not shared.material.lit) {
            context.refuse("eltanin::views::Blueprints::addAssets: shared materials missing");
            return false;
        }
        with<Assets>::add_texpack_catalog(context, Name::from("Eltanin", "mech"), item<rmmr::resource::texpack::LoaderCatalog>{.directory = "textures/mech"}, index2{1024, 1024}, 32);

        const auto hullShader = with<Assets>::add_shader_loader(context, Name::from("Eltanin", "hull"), item<rmmr::resource::shader::Loader>{.vertex = "shaders/hull.vert.glsl", .fragment = "shaders/hull.frag.glsl"});
        if (not rmmr::resource::builders::material::derive(context, Derived{
            .name = Name::from("Eltanin", "hull"),
            .source = *shared.material.litTextured,
            .sourcePass = renderer::Pass::opaque,
            .targetPass = renderer::Pass::opaque,
            .program = with<rmmr::resource::Unit>::remember(context, hullShader),
            .glowSpread = true,
            .renderState = {},
        })) return false;

        const auto wreckShader = with<Assets>::add_shader_loader(context, Name::from("Eltanin", "wreck"), item<rmmr::resource::shader::Loader>{.vertex = "shaders/wreck.vert.glsl", .fragment = "shaders/wreck.frag.glsl"});
        const auto wreckProgram = with<rmmr::resource::Unit>::remember(context, wreckShader);
        if (not rmmr::resource::builders::material::derive(context, Derived{
            .name = Name::from("Eltanin", "wreck"),
            .source = *shared.material.litTextured,
            .sourcePass = renderer::Pass::opaque,
            .targetPass = renderer::Pass::opaque,
            .program = wreckProgram,
            .glowSpread = true,
            .renderState = {},
        })) return false;
        if (not rmmr::resource::builders::material::derive(context, Derived{
            .name = Name::from("Eltanin", "dustWreck"),
            .source = *shared.material.litTexturedAlpha,
            .sourcePass = renderer::Pass::transparent,
            .targetPass = renderer::Pass::transparent,
            .program = wreckProgram,
            .glowSpread = true,
            .renderState = inheritedDepth(renderer::BlendMode::alpha),
        })) return false;

        with<Assets>::add_material(context, Name::from("Eltanin", "type"), with<Material>::get(context, *shared.material.litTransparent));
        with<Assets>::add_material(context, Name::from("Eltanin", "typeSolid"), with<Material>::get(context, *shared.material.lit));
        if (not rmmr::resource::builders::material::derive(context, Derived{
            .name = Name::from("Eltanin", "clipboardGhost"),
            .source = *shared.material.litTransparent,
            .sourcePass = renderer::Pass::transparent,
            .targetPass = renderer::Pass::transparent,
            .program = {},
            .glowSpread = false,
            .renderState = inheritedDepth(renderer::BlendMode::additive),
        })) return false;

        for (const auto& spec : meshpackSpecs)
            with<Assets>::add_meshpack_lwo_loader(context, Name::from("", spec.name), item<rmmr::resource::meshpack::LoaderLwo>{.file = spec.file, .geometry = {}, .pending = {}});
        return true;
    }

}
