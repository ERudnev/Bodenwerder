#include <rmmr/wrapper/library.h>

#include <base/logging.h>
#include <rmmr/api/_interface.h>
#include <rmmr/resources/builders/materialPresets.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/semantics.q1.h>

namespace rmmr::wrapper::assets {

    using namespace fqsm::api;
    using namespace rmmr;

    auto Manager::statePath(const std::filesystem::path& assets_root) -> Location {
        return assets_root / "Toy" / "state" / "resources" / "catalogue.json";
    }

    auto Manager::prepare(Writing context, Location) -> PrepareStatus {
        addHardcoded(context);
        return PrepareStatus::Generated;
    }

    void Manager::addHardcoded(Writing context) {
        using namespace resource;

        base::message("toy: adding hardcoded assets...");

        // Catalog dir name = pack own name; layer names = filenames (debug02.jpg, …).
        handles.texture.debug = with<Assets>::add_texpack_catalog(
            context,
            Unit::Name::from("rmmr", "debug"),
            item<texpack::LoaderCatalog>{.directory = "textures/debug"},
            index2{1024, 1024},
            32);
        handles.texture.whiteCircle = with<Assets>::add_texture_generator(context, Unit::Name::from("rmmr", "white_circle"), item<texture::Generator>{.size = index2{256, 256}, .pattern = texture::Generator::Pattern::whiteCircle});
        handles.texture.whiteRing = with<Assets>::add_texture_generator(context, Unit::Name::from("rmmr", "white_ring"), item<texture::Generator>{.size = index2{256, 256}, .pattern = texture::Generator::Pattern::whiteRing});

        const auto ambient = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "ambient"), item<shader::Loader>{.vertex = "shaders/ambient.vert.glsl", .fragment = "shaders/ambient.frag.glsl"});
        const auto vertex_color = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "vertex_color"), item<shader::Loader>{.vertex = "shaders/vertexColor.vert.glsl", .fragment = "shaders/vertexColor.frag.glsl"});
        const auto gizmo_textured = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "gizmo_textured"), item<shader::Loader>{.vertex = "shaders/gizmoTextured.vert.glsl", .fragment = "shaders/gizmoTextured.frag.glsl"});
        const auto lit = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "lit"), item<shader::Loader>{.vertex = "shaders/lit.vert.glsl", .fragment = "shaders/lit.frag.glsl"});
        const auto lit_transparent = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "lit_transparent"), item<shader::Loader>{.vertex = "shaders/lit.vert.glsl", .fragment = "shaders/litTransparent.frag.glsl"});
        const auto lit_textured = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "lit_textured"), item<shader::Loader>{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTextured.frag.glsl"});
        const auto lit_textured_alpha = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "lit_textured_alpha"), item<shader::Loader>{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTexturedAlpha.frag.glsl"});
        const auto glass = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "glass"), item<shader::Loader>{.vertex = "shaders/glass.vert.glsl", .fragment = "shaders/glass.frag.glsl"});
        const auto grid = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "grid"), item<shader::Loader>{.vertex = "shaders/Grid.vert.glsl", .fragment = "shaders/Grid.frag.glsl"});
        const auto sprite = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "sprite"), item<shader::Loader>{.vertex = "shaders/sprite.vert.glsl", .fragment = "shaders/sprite.frag.glsl"});
        const auto shadow_depth = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "shadow_depth"), item<shader::Loader>{.vertex = "shaders/shadowDepth.vert.glsl", .fragment = "shaders/shadowDepth.frag.glsl"});
        const auto identity = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "identity"), item<shader::Loader>{.vertex = "shaders/identity.vert.glsl", .fragment = "shaders/identity.frag.glsl"});
        const auto family = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "family"), item<shader::Loader>{.vertex = "shaders/family.vert.glsl", .fragment = "shaders/lit.frag.glsl"});
        const auto family_shadow = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "family_shadow"), item<shader::Loader>{.vertex = "shaders/familyShadow.vert.glsl", .fragment = "shaders/shadowDepth.frag.glsl"});

        handles.material.gizmo.textured = with<Assets>::add_material(context, Unit::Name::from("rmmr", "gizmo_textured"), builders::material::Presets::gizmoTextured(with<Unit>::remember(context, gizmo_textured)));
        handles.material.gizmo.vertexColor = with<Assets>::add_material(context, Unit::Name::from("rmmr", "gizmo_vertex_color"), builders::material::Presets::gizmoVertexColor(with<Unit>::remember(context, vertex_color)));
        handles.material.ambient = with<Assets>::add_material(context, Unit::Name::from("rmmr", "ambient"), builders::material::Presets::ambient(with<Unit>::remember(context, ambient), with<Unit>::remember(context, shadow_depth)));
        handles.material.lit = with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit"), builders::material::Presets::lit(with<Unit>::remember(context, lit), with<Unit>::remember(context, shadow_depth)));
        handles.material.litTransparent = with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_transparent"), builders::material::Presets::litTransparent(with<Unit>::remember(context, lit_transparent)));
        handles.material.litTextured = with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_textured"), builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured), with<Unit>::remember(context, shadow_depth)));
        handles.material.litTexturedAlpha = with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_textured_alpha"), builders::material::Presets::litTexturedTransparent(with<Unit>::remember(context, lit_textured_alpha)));
        handles.material.oneSidedGlass = with<Assets>::add_material(context, Unit::Name::from("rmmr", "one_sided_glass"), builders::material::Presets::oneSidedGlass(with<Unit>::remember(context, glass)));
        handles.material.grid = with<Assets>::add_material(context, Unit::Name::from("rmmr", "grid"), builders::material::Presets::grid(with<Unit>::remember(context, grid)));
        handles.material.sprite = with<Assets>::add_material(context, Unit::Name::from("rmmr", "sprite"), builders::material::Presets::sprite(with<Unit>::remember(context, sprite)));
        handles.material.identity = with<Assets>::add_material(context, Unit::Name::from("rmmr", "identity"), builders::material::Presets::identity(with<Unit>::remember(context, identity)));
        handles.material.family = with<Assets>::add_material(context, Unit::Name::from("rmmr", "family"), builders::material::Presets::lit(with<Unit>::remember(context, family), with<Unit>::remember(context, family_shadow)));
        with<scene::actor::Identified>::modify_global(context)->material = handles.material.identity;

        const auto default_blur = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "defaultBlur"), item<shader::Loader>{.vertex = "shaders/overlayDefaultBlur.vert.glsl", .fragment = "shaders/overlayDefaultBlur.frag.glsl"});
        handles.overlay.defaultBlur = with<Assets>::add_overlay(context, Unit::Name::from("rmmr", "defaultBlur"), overlay::Asset::Quantum{
            .program = with<Unit>::remember(context, default_blur),
            .uniforms = ::rmmr::material::Semantics::ids_of({"sceneColor", "identiffyMap", "texelSize"}),
            .scale = overlay::Scale::full,
        });

        handles.primitives = with<Assets>::add_meshpack_objs_loader(context, Unit::Name::from("rmmr", "primitives"), item<meshpack::LoaderObjs>{.file = "meshes/primitives/primitives.meshpack", .pending = {}});

        base::message("toy: hardcoded assets added");
    }

    bool Manager::loadFrom(Stewarding, Location) {
        base::message("toy: assets catalogue load archived (Retrospection offline)");
        return false;
    }

}
