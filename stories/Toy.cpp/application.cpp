#include "application.h"

#include <base/logging.h>
#include <base/maybe.h>
#include <rmmr/controller/camera.q1.h>
#include <rmmr/resources/builders/materialPresets.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/actor.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/interface.q1.h>

#include "projection/world.q1.h"
#include "ui.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace toy {
    using namespace fqsm::api;
    using namespace rmmr;

    struct Application::State : establish::Module::State {
        struct {
            struct {
                std::vector<resource::texture::Asset::Id> debug;
                maybe<resource::texture::Asset::Id> whiteCircle;
                maybe<resource::texture::Asset::Id> whiteRing;
            } texture;
            struct {
                maybe<resource::material::Asset::Id> ambient;
                maybe<resource::material::Asset::Id> lit;
                std::vector<resource::material::Asset::Id> debugLitTextured;
                maybe<resource::material::Asset::Id> litTexturedAlpha;
                maybe<resource::material::Asset::Id> grid;
            } material;
            struct {
                maybe<resource::geometry::Asset::Id> triangle;
                maybe<resource::geometry::Asset::Id> kube;
                maybe<resource::geometry::Asset::Id> bagel;
                maybe<resource::geometry::Asset::Id> grid;
            } primitive;
        } handles;

        ui::State ui;
        establish::Realm world;

        explicit State(Schema schema);
        void setupAsDefault(Writing, establish::Module::RootId&);
        void loadPastState(Writing) override;
        void loadHandles(Writing, system::Core::Id);
        void spawnDemoScene(Writing);
    };

    Application::State::State(Schema schema)
        : establish::Module::State(std::move(schema))
        , world(fullSchema)
    {}

    void Application::State::loadHandles(Writing context, system::Core::Id assets) {
        using resource::Assets;
        using resource::Unit;
        using resource::geometry::Generator;

        base::message("toy: loading assets...");

        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, assets,
            Unit::Quantum{.manager = assets, .name = "debug01", .library = "rmmr"},
            resource::texture::Asset::Quantum{},
            resource::texture::Loader::Quantum{.file = "textures/debug01.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, assets,
            Unit::Quantum{.manager = assets, .name = "debug02", .library = "rmmr"},
            resource::texture::Asset::Quantum{},
            resource::texture::Loader::Quantum{.file = "textures/debug02.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, assets,
            Unit::Quantum{.manager = assets, .name = "debug03", .library = "rmmr"},
            resource::texture::Asset::Quantum{},
            resource::texture::Loader::Quantum{.file = "textures/debug03.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, assets,
            Unit::Quantum{.manager = assets, .name = "debug04", .library = "rmmr"},
            resource::texture::Asset::Quantum{},
            resource::texture::Loader::Quantum{.file = "textures/debug04.jpg"}));
        handles.texture.whiteCircle = with<Assets>::add_texture_generator(
            context, assets,
            Unit::Quantum{.manager = assets, .name = "white_circle", .library = "rmmr"},
            resource::texture::Asset::Quantum{},
            resource::texture::Generator::Quantum{.size = index2{256, 256}, .pattern = resource::texture::Generator::Pattern::whiteCircle});
        handles.texture.whiteRing = with<Assets>::add_texture_generator(
            context, assets,
            Unit::Quantum{.manager = assets, .name = "white_ring", .library = "rmmr"},
            resource::texture::Asset::Quantum{},
            resource::texture::Generator::Quantum{.size = index2{256, 256}, .pattern = resource::texture::Generator::Pattern::whiteRing});

        const auto ambient_shader = with<Assets>::add_shader_loader(context, assets, Unit::Quantum{.manager = assets, .name = "ambient_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/ambient.vert.glsl", .fragment = "shaders/ambient.frag.glsl"});
        const auto lit_shader = with<Assets>::add_shader_loader(context, assets, Unit::Quantum{.manager = assets, .name = "lit_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/lit.vert.glsl", .fragment = "shaders/lit.frag.glsl"});
        const auto lit_textured_shader = with<Assets>::add_shader_loader(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTextured.frag.glsl"});
        const auto lit_textured_alpha_shader = with<Assets>::add_shader_loader(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_alpha_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTexturedAlpha.frag.glsl"});
        const auto grid_shader = with<Assets>::add_shader_loader(context, assets, Unit::Quantum{.manager = assets, .name = "grid_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/Grid.vert.glsl", .fragment = "shaders/Grid.frag.glsl"});
        const auto shadow_depth_shader = with<Assets>::add_shader_loader(context, assets, Unit::Quantum{.manager = assets, .name = "shadow_depth_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/shadowDepth.vert.glsl", .fragment = "shaders/shadowDepth.frag.glsl"});

        handles.material.ambient = with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "ambient_material", .library = "rmmr"}, resource::builders::material::MaterialPresets::ambient(ambient_shader, shadow_depth_shader), resource::material::Composer::Quantum{});
        handles.material.lit = with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_material", .library = "rmmr"}, resource::builders::material::MaterialPresets::lit(lit_shader, shadow_depth_shader), resource::material::Composer::Quantum{});
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug01", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, handles.texture.debug[0], shadow_depth_shader), resource::material::Composer::Quantum{}));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug02", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, handles.texture.debug[1], shadow_depth_shader), resource::material::Composer::Quantum{}));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug03", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, handles.texture.debug[2], shadow_depth_shader), resource::material::Composer::Quantum{}));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug04", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, handles.texture.debug[3], shadow_depth_shader), resource::material::Composer::Quantum{}));
        handles.material.litTexturedAlpha = with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_alpha_ring", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTexturedTransparent(lit_textured_alpha_shader, *handles.texture.whiteRing), resource::material::Composer::Quantum{});
        handles.material.grid = with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "grid_material", .library = "rmmr"}, resource::builders::material::MaterialPresets::grid(grid_shader), resource::material::Composer::Quantum{});

        handles.primitive.triangle = with<Assets>::add_geometry_generator(context, assets, Unit::Quantum{.manager = assets, .name = "triangle", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::triangle});
        handles.primitive.kube = with<Assets>::add_geometry_generator(context, assets, Unit::Quantum{.manager = assets, .name = "kube", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::kube});
        handles.primitive.bagel = with<Assets>::add_geometry_generator(context, assets, Unit::Quantum{.manager = assets, .name = "bagel", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::bagel});
        handles.primitive.grid = with<Assets>::add_geometry_generator(context, assets, Unit::Quantum{.manager = assets, .name = "grid", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::gridPlane});

        base::message("toy: assets loaded");
    }

    void Application::State::spawnDemoScene(Writing context) {
        const auto root = with<scene::Interface>::createScene(context);

        constexpr int grid_extent = 4;
        constexpr float cube_edge = 1.0f;
        constexpr float spacing = cube_edge * 1.5f;
        const float center_offset = (static_cast<float>(grid_extent) - 1.0f) * 0.5f;
        const float cluster_lift = center_offset * spacing + cube_edge * 0.5f;

        for (int z = 0; z < grid_extent; ++z) {
            for (int y = 0; y < grid_extent; ++y) {
                for (int x = 0; x < grid_extent; ++x) {
                    const int cell = x + y + z;
                    const bool alpha_cutout = (cell % 5 == 0);
                    const Pos pos{
                        (static_cast<float>(x) - center_offset) * spacing,
                        (static_cast<float>(y) - center_offset) * spacing + cluster_lift,
                        (static_cast<float>(z) - center_offset) * spacing,
                    };
                    with<scene::Interface>::createPrimitiveActor(context, root,
                        Locator{
                            .pos = pos,
                            .euler = HPB{
                                -22.5f + 45.0f * static_cast<float>(x),
                                -15.0f + 30.0f * static_cast<float>(y),
                                -12.0f + 24.0f * static_cast<float>(z),
                            },
                        },
                        scene::PrimitiveActor::Quantum{
                            .geometry = (cell % 7 == 0) ? *handles.primitive.bagel : *handles.primitive.kube,
                            .material = alpha_cutout ? *handles.material.litTexturedAlpha : handles.material.debugLitTextured[cell % 4],
                            .albedo = RGB{
                                0.3f + 0.6f * static_cast<float>(x) / static_cast<float>(grid_extent - 1),
                                0.3f + 0.6f * static_cast<float>(y) / static_cast<float>(grid_extent - 1),
                                0.3f + 0.6f * static_cast<float>(z) / static_cast<float>(grid_extent - 1),
                            },
                        });
                }
            }
        }

        with<scene::Interface>::createGrid(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 0.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            scene::Grid::Quantum{.geometry = *handles.primitive.grid, .material = *handles.material.grid, .opacity = 1.0f});

        ui.camera = with<scene::Interface>::createCamera(context, root,
            Locator{.pos = Pos{10.5f, 10.0f, 14.0f}, .euler = HPB{-18.0f, -36.0f, 0.0f}},
            scene::Camera::Quantum{.fov_y = 1.04719755f, .z_near = 0.1f, .z_far = 100.0f});
        with<controller::Camera>::create(context, ui.camera);
        with<scene::Interface>::createLight(context, root,
            Locator{.pos = Pos{9.5f, 19.0f, 7.5f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            scene::Light::Quantum{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 30.0f});

        ui.scene = root;
    }

    void Application::State::setupAsDefault(Writing context, establish::Module::RootId& root) {
        const auto core = root.secretGet<system::Core>();
        if (not core.exists())
            throw std::runtime_error("app: RootId is not system::Core");
        loadHandles(context, *core);
        spawnDemoScene(context);
    }

    void Application::State::loadPastState(Writing) {
        _INCOMPLETE_;
    }


    //
    // Application
    //

    Application::Application(Settings settings)
        : settings(std::move(settings))
        , engine(add<rmmr::Engine>())
    {}

    Application::~Application() = default;

    Schema Application::schema() {
        static const Schema native = ask::schema::aspect<God>();
        Schema result = native;
        for (auto& child : submodules)
            result = ask::schema::merge({result, child->schema()});
        return result;
    }

    void Application::install(Schema schema) {
        state = std::make_shared<State>(std::move(schema));
        for (auto& child : submodules)
            child->installState(state->fullSchema);
    }

    void Application::initDefaultWorld() {
        establish::Module::RootId root;

        base::message("app: creating core...");
        const auto core = with<system::Interface>::create(
            state->world,
            settings.assets_root,
            system::Core::GLVer{
                .major = settings.glVersion.major,
                .minor = settings.glVersion.minor,
            });
        root.secretSet(core);

        engine->setup(state->world, root, Engine::WindowParameters{
            .title = settings.title,
            .requested_size = settings.window_size,
        });
        state->setupAsDefault(state->world, root);
        engine->materialize(state->world, core);
        engine->showScene(*state->ui.scene, *state->ui.camera);

        if (not state->world.result().good())
            throw std::runtime_error("app: initDefaultWorld failed");
    }

    void Application::loadWorld(filepath) {
        state->loadPastState(state->world);
        if (not state->world.result().good())
            throw std::runtime_error("app: loadWorld failed");
    }

    int Application::run() {
        while (engine and not engine->shouldClose(state->world)) {
            engine->beginFrame(state->world);
            state->ui.draw(state->world);
            engine->render(state->world);
            engine->endFrame(state->world);
        }

        if (engine)
            engine->shutdown(state->world);
        return 0;
    }

}
