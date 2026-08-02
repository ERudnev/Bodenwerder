#include <rmmr/wrapper/library.h>

#include <base/logging.h>
#include <rmmr/api/_interface.h>
#include <rmmr/resources/builders/materialPresets.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>

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

        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context,
            item<Unit>{.name = "debug01", .library = "rmmr"},
            item<texture::Loader>{.file = "textures/debug01.jpg", .mipmaps = true}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context,
            item<Unit>{.name = "debug02", .library = "rmmr"},
            item<texture::Loader>{.file = "textures/debug02.jpg", .mipmaps = true}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context,
            item<Unit>{.name = "debug03", .library = "rmmr"},
            item<texture::Loader>{.file = "textures/debug03.jpg", .mipmaps = true}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context,
            item<Unit>{.name = "debug04", .library = "rmmr"},
            item<texture::Loader>{.file = "textures/debug04.jpg", .mipmaps = true}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context,
            item<Unit>{.name = "debug05", .library = "rmmr"},
            item<texture::Loader>{.file = "textures/debug05.jpg", .mipmaps = true}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context,
            item<Unit>{.name = "debug06", .library = "rmmr"},
            item<texture::Loader>{.file = "textures/debug06.jpg", .mipmaps = true}));
        handles.texture.whiteCircle = with<Assets>::add_texture_generator(
            context,
            item<Unit>{.name = "white_circle", .library = "rmmr"},
            item<texture::Generator>{.size = index2{256, 256}, .pattern = texture::Generator::Pattern::whiteCircle});
        handles.texture.whiteRing = with<Assets>::add_texture_generator(
            context,
            item<Unit>{.name = "white_ring", .library = "rmmr"},
            item<texture::Generator>{.size = index2{256, 256}, .pattern = texture::Generator::Pattern::whiteRing});

        const auto ambient_shader = with<Assets>::add_shader_loader(context, item<Unit>{.name = "ambient_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/ambient.vert.glsl", .fragment = "shaders/ambient.frag.glsl"});
        const auto vertex_color_shader = with<Assets>::add_shader_loader(context, item<Unit>{.name = "vertex_color_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/vertexColor.vert.glsl", .fragment = "shaders/vertexColor.frag.glsl"});
        const auto gizmo_textured_shader = with<Assets>::add_shader_loader(context, item<Unit>{.name = "gizmo_textured_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/gizmoTextured.vert.glsl", .fragment = "shaders/gizmoTextured.frag.glsl"});
        const auto lit_shader = with<Assets>::add_shader_loader(context, item<Unit>{.name = "lit_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/lit.vert.glsl", .fragment = "shaders/lit.frag.glsl"});
        const auto lit_textured_shader = with<Assets>::add_shader_loader(context, item<Unit>{.name = "lit_textured_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTextured.frag.glsl"});
        const auto lit_textured_alpha_shader = with<Assets>::add_shader_loader(context, item<Unit>{.name = "lit_textured_alpha_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTexturedAlpha.frag.glsl"});
        const auto glass_shader = with<Assets>::add_shader_loader(context, item<Unit>{.name = "glass_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/glass.vert.glsl", .fragment = "shaders/glass.frag.glsl"});
        const auto grid_shader = with<Assets>::add_shader_loader(context, item<Unit>{.name = "grid_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/Grid.vert.glsl", .fragment = "shaders/Grid.frag.glsl"});
        const auto sprite_shader = with<Assets>::add_shader_loader(context, item<Unit>{.name = "sprite_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/sprite.vert.glsl", .fragment = "shaders/sprite.frag.glsl"});
        const auto shadow_depth_shader = with<Assets>::add_shader_loader(context, item<Unit>{.name = "shadow_depth_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/shadowDepth.vert.glsl", .fragment = "shaders/shadowDepth.frag.glsl"});

        handles.geometry.windowedKube = with<Assets>::add_geometry_generator(context, item<Unit>{.name = "windowed_kube", .library = "rmmr"}, item<geometry::Generator>{.type = geometry::Generator::Type::windowedKube});
        // Matches GeometryGenerator::windowedKube index layout: all outer tris, then all window tris.
        with<geometry::Asset>::modify(context, handles.geometry.windowedKube)->parts = {
            {"outer", {.startIndex = 0, .countIndex = 6 * 24}},
            {"window", {.startIndex = 6 * 24, .countIndex = 6 * 6}},
        };

        handles.material.gizmo.textured = with<Assets>::add_material(context, item<Unit>{.name = "gizmo_textured_debug06", .library = "rmmr"}, builders::material::Presets::gizmoTextured(with<Unit>::remember(context, gizmo_textured_shader), with<Unit>::remember(context, handles.texture.debug[5])));
        handles.material.gizmo.vertexColor = with<Assets>::add_material(context, item<Unit>{.name = "gizmo_vertex_color", .library = "rmmr"}, builders::material::Presets::gizmoVertexColor(with<Unit>::remember(context, vertex_color_shader)));
        handles.material.ambient = with<Assets>::add_material(context, item<Unit>{.name = "ambient_material", .library = "rmmr"}, builders::material::Presets::ambient(with<Unit>::remember(context, ambient_shader), with<Unit>::remember(context, shadow_depth_shader)));
        handles.material.lit = with<Assets>::add_material(context, item<Unit>{.name = "lit_material", .library = "rmmr"}, builders::material::Presets::lit(with<Unit>::remember(context, lit_shader), with<Unit>::remember(context, shadow_depth_shader)));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, item<Unit>{.name = "lit_textured_debug01", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[0]), with<Unit>::remember(context, shadow_depth_shader))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, item<Unit>{.name = "lit_textured_debug02", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[1]), with<Unit>::remember(context, shadow_depth_shader))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, item<Unit>{.name = "lit_textured_debug03", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[2]), with<Unit>::remember(context, shadow_depth_shader))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, item<Unit>{.name = "lit_textured_debug04", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[3]), with<Unit>::remember(context, shadow_depth_shader))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, item<Unit>{.name = "lit_textured_debug05", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[4]), with<Unit>::remember(context, shadow_depth_shader))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, item<Unit>{.name = "lit_textured_debug06", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[5]), with<Unit>::remember(context, shadow_depth_shader))));
        handles.material.litTexturedAlpha = with<Assets>::add_material(context, item<Unit>{.name = "lit_textured_alpha_ring", .library = "rmmr"}, builders::material::Presets::litTexturedTransparent(with<Unit>::remember(context, lit_textured_alpha_shader), with<Unit>::remember(context, *handles.texture.whiteRing)));
        handles.material.oneSidedGlass = with<Assets>::add_material(context, item<Unit>{.name = "one_sided_glass_debug06", .library = "rmmr"}, builders::material::Presets::oneSidedGlass(with<Unit>::remember(context, glass_shader), with<Unit>::remember(context, handles.texture.debug[5])));
        handles.material.grid = with<Assets>::add_material(context, item<Unit>{.name = "grid_material", .library = "rmmr"}, builders::material::Presets::grid(with<Unit>::remember(context, grid_shader)));
        handles.material.sprite = with<Assets>::add_material(context, item<Unit>{.name = "sprite_material", .library = "rmmr"}, builders::material::Presets::sprite(with<Unit>::remember(context, sprite_shader)));
        handles.meshpack = with<Assets>::add_meshpack_loader(context, item<Unit>{.name = "default", .library = "rmmr"}, item<meshpack::Loader>{.file = "meshes/etalon/default.meshpack"});

        base::message("toy: hardcoded assets added");
    }

    bool Manager::loadFrom(Stewarding, Location) {
        base::message("toy: assets catalogue load archived (Retrospection offline)");
        return false;
    }

}
