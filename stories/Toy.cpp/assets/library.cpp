#include "assets/library.h"

#include <base/logging.h>
#include <rmmr/resources/builders/materialPresets.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>

namespace toy::assets {

    using namespace fqsm::api;
    using namespace rmmr;

    void Manager::hardcodedInit(Writing context, system::Core::Id assets) {
        using namespace resource;
        using geometry::Generator;

        base::message("toy: seeding assets (hardcoded)...");

        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, assets,
            item<Unit>{.manager = assets, .name = "debug01", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Loader>{.file = "textures/debug01.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, assets,
            item<Unit>{.manager = assets, .name = "debug02", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Loader>{.file = "textures/debug02.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, assets,
            item<Unit>{.manager = assets, .name = "debug03", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Loader>{.file = "textures/debug03.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, assets,
            item<Unit>{.manager = assets, .name = "debug04", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Loader>{.file = "textures/debug04.jpg"}));
        handles.texture.whiteCircle = with<Assets>::add_texture_generator(
            context, assets,
            item<Unit>{.manager = assets, .name = "white_circle", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Generator>{.size = index2{256, 256}, .pattern = texture::Generator::Pattern::whiteCircle});
        handles.texture.whiteRing = with<Assets>::add_texture_generator(
            context, assets,
            item<Unit>{.manager = assets, .name = "white_ring", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Generator>{.size = index2{256, 256}, .pattern = texture::Generator::Pattern::whiteRing});

        const auto ambient_shader = with<Assets>::add_shader_loader(context, assets, item<Unit>{.manager = assets, .name = "ambient_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/ambient.vert.glsl", .fragment = "shaders/ambient.frag.glsl"});
        const auto lit_shader = with<Assets>::add_shader_loader(context, assets, item<Unit>{.manager = assets, .name = "lit_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/lit.vert.glsl", .fragment = "shaders/lit.frag.glsl"});
        const auto lit_textured_shader = with<Assets>::add_shader_loader(context, assets, item<Unit>{.manager = assets, .name = "lit_textured_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTextured.frag.glsl"});
        const auto lit_textured_alpha_shader = with<Assets>::add_shader_loader(context, assets, item<Unit>{.manager = assets, .name = "lit_textured_alpha_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTexturedAlpha.frag.glsl"});
        const auto grid_shader = with<Assets>::add_shader_loader(context, assets, item<Unit>{.manager = assets, .name = "grid_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/Grid.vert.glsl", .fragment = "shaders/Grid.frag.glsl"});
        const auto shadow_depth_shader = with<Assets>::add_shader_loader(context, assets, item<Unit>{.manager = assets, .name = "shadow_depth_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/shadowDepth.vert.glsl", .fragment = "shaders/shadowDepth.frag.glsl"});

        handles.material.ambient = with<Assets>::add_material(context, assets, item<Unit>{.manager = assets, .name = "ambient_material", .library = "rmmr"}, builders::material::Presets::ambient(with<Unit>::remember(context, ambient_shader), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{});
        handles.material.lit = with<Assets>::add_material(context, assets, item<Unit>{.manager = assets, .name = "lit_material", .library = "rmmr"}, builders::material::Presets::lit(with<Unit>::remember(context, lit_shader), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{});
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, item<Unit>{.manager = assets, .name = "lit_textured_debug01", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[0]), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{}));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, item<Unit>{.manager = assets, .name = "lit_textured_debug02", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[1]), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{}));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, item<Unit>{.manager = assets, .name = "lit_textured_debug03", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[2]), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{}));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, item<Unit>{.manager = assets, .name = "lit_textured_debug04", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[3]), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{}));
        handles.material.litTexturedAlpha = with<Assets>::add_material(context, assets, item<Unit>{.manager = assets, .name = "lit_textured_alpha_ring", .library = "rmmr"}, builders::material::Presets::litTexturedTransparent(with<Unit>::remember(context, lit_textured_alpha_shader), with<Unit>::remember(context, *handles.texture.whiteRing)), item<resource::material::Composer>{});
        handles.material.grid = with<Assets>::add_material(context, assets, item<Unit>{.manager = assets, .name = "grid_material", .library = "rmmr"}, builders::material::Presets::grid(with<Unit>::remember(context, grid_shader)), item<resource::material::Composer>{});

        handles.primitive.triangle = with<Assets>::add_geometry_generator(context, assets, item<Unit>{.manager = assets, .name = "triangle", .library = "rmmr"}, item<geometry::Asset>{}, item<Generator>{.type = Generator::Type::triangle});
        handles.primitive.kube = with<Assets>::add_geometry_generator(context, assets, item<Unit>{.manager = assets, .name = "kube", .library = "rmmr"}, item<geometry::Asset>{}, item<Generator>{.type = Generator::Type::kube});
        handles.primitive.bagel = with<Assets>::add_geometry_generator(context, assets, item<Unit>{.manager = assets, .name = "bagel", .library = "rmmr"}, item<geometry::Asset>{}, item<Generator>{.type = Generator::Type::bagel});
        handles.primitive.grid = with<Assets>::add_geometry_generator(context, assets, item<Unit>{.manager = assets, .name = "grid", .library = "rmmr"}, item<geometry::Asset>{}, item<Generator>{.type = Generator::Type::gridPlane});

        base::message("toy: assets seeded");
    }

    bool Manager::loadFrom(Stewarding, Location) {
        _INCOMPLETE_;
    }

    void Manager::save(Reading, Location) {
        _INCOMPLETE_;
    }

}
