#include "assets/library.h"

#include <base/logging.h>
#include <rmmr/api/_interface.h>
#include <rmmr/resources/builders/materialPresets.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>

namespace toy::assets {

    using namespace fqsm::api;
    using namespace rmmr;

    auto Manager::statePath(const std::filesystem::path& assets_root) -> Location {
        return assets_root / "Toy" / "state" / "resources" / "catalogue.json";
    }

    auto Manager::prepare(establish::Realm& world, Location) -> PrepareStatus {
        // Always reseed from hardcoded presets. Catalogue persistency is archived.
        hardcodedInit(world);
        return PrepareStatus::Generated;
    }

    void Manager::hardcodedInit(Writing context) {
        using namespace resource;

        base::message("toy: seeding assets (hardcoded)...");

        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, core,
            item<Unit>{.manager = core, .name = "debug01", .library = "rmmr"},
            item<texture::Loader>{.file = "textures/debug01.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, core,
            item<Unit>{.manager = core, .name = "debug02", .library = "rmmr"},
            item<texture::Loader>{.file = "textures/debug02.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, core,
            item<Unit>{.manager = core, .name = "debug03", .library = "rmmr"},
            item<texture::Loader>{.file = "textures/debug03.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, core,
            item<Unit>{.manager = core, .name = "debug04", .library = "rmmr"},
            item<texture::Loader>{.file = "textures/debug04.jpg"}));
        handles.texture.whiteCircle = with<Assets>::add_texture_generator(
            context, core,
            item<Unit>{.manager = core, .name = "white_circle", .library = "rmmr"},
            item<texture::Generator>{.size = index2{256, 256}, .pattern = texture::Generator::Pattern::whiteCircle});
        handles.texture.whiteRing = with<Assets>::add_texture_generator(
            context, core,
            item<Unit>{.manager = core, .name = "white_ring", .library = "rmmr"},
            item<texture::Generator>{.size = index2{256, 256}, .pattern = texture::Generator::Pattern::whiteRing});

        const auto ambient_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "ambient_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/ambient.vert.glsl", .fragment = "shaders/ambient.frag.glsl"});
        const auto lit_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "lit_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/lit.vert.glsl", .fragment = "shaders/lit.frag.glsl"});
        const auto lit_textured_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "lit_textured_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTextured.frag.glsl"});
        const auto lit_textured_alpha_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "lit_textured_alpha_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTexturedAlpha.frag.glsl"});
        const auto grid_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "grid_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/Grid.vert.glsl", .fragment = "shaders/Grid.frag.glsl"});
        const auto sprite_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "sprite_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/sprite.vert.glsl", .fragment = "shaders/sprite.frag.glsl"});
        const auto shadow_depth_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "shadow_depth_shader", .library = "rmmr"}, item<shader::Loader>{.vertex = "shaders/shadowDepth.vert.glsl", .fragment = "shaders/shadowDepth.frag.glsl"});

        handles.material.ambient = with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "ambient_material", .library = "rmmr"}, builders::material::Presets::ambient(with<Unit>::remember(context, ambient_shader), with<Unit>::remember(context, shadow_depth_shader)));
        handles.material.lit = with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_material", .library = "rmmr"}, builders::material::Presets::lit(with<Unit>::remember(context, lit_shader), with<Unit>::remember(context, shadow_depth_shader)));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_textured_debug01", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[0]), with<Unit>::remember(context, shadow_depth_shader))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_textured_debug02", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[1]), with<Unit>::remember(context, shadow_depth_shader))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_textured_debug03", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[2]), with<Unit>::remember(context, shadow_depth_shader))));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_textured_debug04", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[3]), with<Unit>::remember(context, shadow_depth_shader))));
        handles.material.litTexturedAlpha = with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_textured_alpha_ring", .library = "rmmr"}, builders::material::Presets::litTexturedTransparent(with<Unit>::remember(context, lit_textured_alpha_shader), with<Unit>::remember(context, *handles.texture.whiteRing)));
        handles.material.grid = with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "grid_material", .library = "rmmr"}, builders::material::Presets::grid(with<Unit>::remember(context, grid_shader)));
        handles.material.sprite = with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "sprite_material", .library = "rmmr"}, builders::material::Presets::sprite(with<Unit>::remember(context, sprite_shader)));

        base::message("toy: assets seeded");
    }

    bool Manager::loadFrom(Stewarding, Location) {
        base::message("toy: assets catalogue load archived (Retrospection offline)");
        return false;
    }

    void Manager::save(Writing, Location) {
        // Catalogue persistency archived with Retrospection forms.
    }

}
