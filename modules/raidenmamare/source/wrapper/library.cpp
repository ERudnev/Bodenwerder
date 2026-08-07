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

        handles.texture.debug.push_back(with<Assets>::add_texture_loader(context, Unit::Name::from("rmmr", "debug01"), item<texture::Loader>{.file = "textures/debug01.jpg", .mipmaps = true}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(context, Unit::Name::from("rmmr", "debug02"), item<texture::Loader>{.file = "textures/debug02.jpg", .mipmaps = true}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(context, Unit::Name::from("rmmr", "debug03"), item<texture::Loader>{.file = "textures/debug03.jpg", .mipmaps = true}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(context, Unit::Name::from("rmmr", "debug04"), item<texture::Loader>{.file = "textures/debug04.jpg", .mipmaps = true}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(context, Unit::Name::from("rmmr", "debug05"), item<texture::Loader>{.file = "textures/debug05.jpg", .mipmaps = true}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(context, Unit::Name::from("rmmr", "debug06"), item<texture::Loader>{.file = "textures/debug06.jpg", .mipmaps = true}));
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

        handles.material.gizmo.textured = with<Assets>::add_material(context, Unit::Name::from("rmmr", "gizmo_textured_debug06"), builders::material::Presets::gizmoTextured(with<Unit>::remember(context, gizmo_textured), with<Unit>::remember(context, handles.texture.debug[5])));
        handles.material.gizmo.vertexColor = with<Assets>::add_material(context, Unit::Name::from("rmmr", "gizmo_vertex_color"), builders::material::Presets::gizmoVertexColor(with<Unit>::remember(context, vertex_color)));
        handles.material.ambient = with<Assets>::add_material(context, Unit::Name::from("rmmr", "ambient"), builders::material::Presets::ambient(with<Unit>::remember(context, ambient), with<Unit>::remember(context, shadow_depth)));
        handles.material.lit = with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit"), builders::material::Presets::lit(with<Unit>::remember(context, lit), with<Unit>::remember(context, shadow_depth)));
        handles.material.litTransparent = with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_transparent"), builders::material::Presets::litTransparent(with<Unit>::remember(context, lit_transparent)));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_textured_debug01"), builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured), with<Unit>::remember(context, handles.texture.debug[0]), with<Unit>::remember(context, shadow_depth))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_textured_debug02"), builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured), with<Unit>::remember(context, handles.texture.debug[1]), with<Unit>::remember(context, shadow_depth))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_textured_debug03"), builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured), with<Unit>::remember(context, handles.texture.debug[2]), with<Unit>::remember(context, shadow_depth))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_textured_debug04"), builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured), with<Unit>::remember(context, handles.texture.debug[3]), with<Unit>::remember(context, shadow_depth))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_textured_debug05"), builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured), with<Unit>::remember(context, handles.texture.debug[4]), with<Unit>::remember(context, shadow_depth))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_textured_debug06"), builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured), with<Unit>::remember(context, handles.texture.debug[5]), with<Unit>::remember(context, shadow_depth))));
        handles.material.litTexturedAlpha = with<Assets>::add_material(context, Unit::Name::from("rmmr", "lit_textured_alpha_ring"), builders::material::Presets::litTexturedTransparent(with<Unit>::remember(context, lit_textured_alpha), with<Unit>::remember(context, *handles.texture.whiteRing)));
        handles.material.oneSidedGlass = with<Assets>::add_material(context, Unit::Name::from("rmmr", "one_sided_glass_debug06"), builders::material::Presets::oneSidedGlass(with<Unit>::remember(context, glass), with<Unit>::remember(context, handles.texture.debug[5])));
        handles.material.grid = with<Assets>::add_material(context, Unit::Name::from("rmmr", "grid"), builders::material::Presets::grid(with<Unit>::remember(context, grid)));
        handles.material.sprite = with<Assets>::add_material(context, Unit::Name::from("rmmr", "sprite"), builders::material::Presets::sprite(with<Unit>::remember(context, sprite)));
        handles.material.identity = with<Assets>::add_material(context, Unit::Name::from("rmmr", "identity"), builders::material::Presets::identity(with<Unit>::remember(context, identity)));
        with<scene::actor::Identified>::modify_global(context)->material = handles.material.identity;

        const auto default_blur = with<Assets>::add_shader_loader(context, Unit::Name::from("rmmr", "defaultBlur"), item<shader::Loader>{.vertex = "shaders/overlayDefaultBlur.vert.glsl", .fragment = "shaders/overlayDefaultBlur.frag.glsl"});
        handles.overlay.defaultBlur = with<Assets>::add_overlay(context, Unit::Name::from("rmmr", "defaultBlur"), overlay::Asset::Quantum{
            .program = with<Unit>::remember(context, default_blur),
            .uniforms = ::rmmr::material::Semantics::ids_of({"sceneColor", "identiffyMap", "texelSize"}),
            .scale = overlay::Scale::full,
        });

        handles.primitives = with<Assets>::add_meshpack_objs_loader(context, Unit::Name::from("rmmr", "primitives"), item<meshpack::LoaderObjs>{.file = "meshes/primitives/primitives.meshpack"});

        base::message("toy: hardcoded assets added");
    }

    bool Manager::loadFrom(Stewarding, Location) {
        base::message("toy: assets catalogue load archived (Retrospection offline)");
        return false;
    }

}
