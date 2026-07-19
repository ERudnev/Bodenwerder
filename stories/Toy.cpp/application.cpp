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
#include <rmmr/resources/shadows.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/actor.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/interface.q1.h>

#include "projection/world.q1.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace toy {
    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        struct Catalog {
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
            maybe<resource::shadow::Asset::Id> shadow;
        };

        auto load_catalog(Writing context, system::Core::Id assets) -> Catalog {
            using resource::Assets;
            using resource::Unit;
            using resource::geometry::Generator;

            Catalog catalog;
            base::message("toy: loading assets...");

            catalog.texture.debug.push_back(with<Assets>::add_texture_loader(
                context, assets,
                Unit::Quantum{.manager = assets, .name = "debug01", .library = "rmmr"},
                resource::texture::Asset::Quantum{},
                resource::texture::Loader::Quantum{.file = "textures/debug01.jpg"}));
            catalog.texture.debug.push_back(with<Assets>::add_texture_loader(
                context, assets,
                Unit::Quantum{.manager = assets, .name = "debug02", .library = "rmmr"},
                resource::texture::Asset::Quantum{},
                resource::texture::Loader::Quantum{.file = "textures/debug02.jpg"}));
            catalog.texture.debug.push_back(with<Assets>::add_texture_loader(
                context, assets,
                Unit::Quantum{.manager = assets, .name = "debug03", .library = "rmmr"},
                resource::texture::Asset::Quantum{},
                resource::texture::Loader::Quantum{.file = "textures/debug03.jpg"}));
            catalog.texture.debug.push_back(with<Assets>::add_texture_loader(
                context, assets,
                Unit::Quantum{.manager = assets, .name = "debug04", .library = "rmmr"},
                resource::texture::Asset::Quantum{},
                resource::texture::Loader::Quantum{.file = "textures/debug04.jpg"}));
            catalog.texture.whiteCircle = with<Assets>::add_texture_generator(
                context, assets,
                Unit::Quantum{.manager = assets, .name = "white_circle", .library = "rmmr"},
                resource::texture::Asset::Quantum{},
                resource::texture::Generator::Quantum{.size = index2{256, 256}, .pattern = resource::texture::Generator::Pattern::whiteCircle});
            catalog.texture.whiteRing = with<Assets>::add_texture_generator(
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

            catalog.material.ambient = with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "ambient_material", .library = "rmmr"}, resource::builders::material::MaterialPresets::ambient(ambient_shader, shadow_depth_shader), resource::material::Composer::Quantum{});
            catalog.material.lit = with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_material", .library = "rmmr"}, resource::builders::material::MaterialPresets::lit(lit_shader, shadow_depth_shader), resource::material::Composer::Quantum{});
            catalog.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug01", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, catalog.texture.debug[0], shadow_depth_shader), resource::material::Composer::Quantum{}));
            catalog.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug02", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, catalog.texture.debug[1], shadow_depth_shader), resource::material::Composer::Quantum{}));
            catalog.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug03", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, catalog.texture.debug[2], shadow_depth_shader), resource::material::Composer::Quantum{}));
            catalog.material.debugLitTextured.push_back(with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug04", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, catalog.texture.debug[3], shadow_depth_shader), resource::material::Composer::Quantum{}));
            catalog.material.litTexturedAlpha = with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_alpha_ring", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTexturedTransparent(lit_textured_alpha_shader, *catalog.texture.whiteRing), resource::material::Composer::Quantum{});
            catalog.material.grid = with<Assets>::add_material(context, assets, Unit::Quantum{.manager = assets, .name = "grid_material", .library = "rmmr"}, resource::builders::material::MaterialPresets::grid(grid_shader), resource::material::Composer::Quantum{});

            catalog.primitive.triangle = with<Assets>::add_geometry_generator(context, assets, Unit::Quantum{.manager = assets, .name = "triangle", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::triangle});
            catalog.primitive.kube = with<Assets>::add_geometry_generator(context, assets, Unit::Quantum{.manager = assets, .name = "kube", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::kube});
            catalog.primitive.bagel = with<Assets>::add_geometry_generator(context, assets, Unit::Quantum{.manager = assets, .name = "bagel", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::bagel});
            catalog.primitive.grid = with<Assets>::add_geometry_generator(context, assets, Unit::Quantum{.manager = assets, .name = "grid", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::gridPlane});

            catalog.shadow = with<Assets>::add_shadow_allocator(context, assets, Unit::Quantum{.manager = assets, .name = "main_shadow", .library = "rmmr"}, resource::shadow::Asset::Quantum{}, resource::shadow::Allocator::Quantum{.size = index2{1024, 1024}});

            base::message("toy: assets loaded");
            return catalog;
        }

        auto spawn_demo_scene(Writing context, const Catalog& catalog) -> std::pair<scene::Root::Id, scene::Camera::Id> {
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
                                .geometry = (cell % 7 == 0) ? *catalog.primitive.bagel : *catalog.primitive.kube,
                                .material = alpha_cutout ? *catalog.material.litTexturedAlpha : catalog.material.debugLitTextured[cell % 4],
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
                scene::Grid::Quantum{.geometry = *catalog.primitive.grid, .material = *catalog.material.grid, .opacity = 1.0f});

            const auto camera = with<scene::Interface>::createCamera(context, root,
                Locator{.pos = Pos{10.5f, 10.0f, 14.0f}, .euler = HPB{-18.0f, -36.0f, 0.0f}},
                scene::Camera::Quantum{.fov_y = 1.04719755f, .z_near = 0.1f, .z_far = 100.0f});
            with<controller::Camera>::create(context, camera);
            with<scene::Interface>::createLight(context, root,
                Locator{.pos = Pos{9.5f, 19.0f, 7.5f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
                scene::Light::Quantum{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 30.0f, .shadow = catalog.shadow});

            return {root, camera};
        }

    } // namespace

    struct Application::State : establish::Module::State {
        Application* application;
        std::vector<std::shared_ptr<establish::Module::State>> children;

        State(Schema schema, Application* owner)
            : establish::Module::State(std::move(schema))
            , application(owner)
        {}

        void createDefaultState(Writing context, establish::Module::RootId& root) override {
            base::message("app: creating core...");
            const auto core = with<system::Interface>::create(
                context,
                application->settings.assets_root,
                system::Core::GLVer{
                    .major = application->settings.glVersion.major,
                    .minor = application->settings.glVersion.minor,
                });
            root.secretSet(core);

            for (auto& child : children)
                child->createDefaultState(context, root);

            const auto catalog = load_catalog(context, core);
            application->engine->materialize(context, core);
            const auto [scene, camera] = spawn_demo_scene(context, catalog);
            application->engine->setScene(scene, camera);
        }

        void loadPastState(Writing) override {}
    };

    Application::Application(Settings settings)
        : settings(std::move(settings))
    {}

    Application::~Application() = default;

    Schema Application::domain() {
        return ask::schema::aspect<God>();
    }

    void Application::install(Schema schema) {
        state = std::static_pointer_cast<State>(installState(std::move(schema)));
    }

    std::shared_ptr<establish::Module::State> Application::installState(Schema finalSchema) {
        auto installed = std::make_shared<State>(std::move(finalSchema), this);
        for (auto& child : modules_)
            installed->children.push_back(child->installState(installed->fullSchema));
        return installed;
    }

    void Application::prepareEngineWindow() {
        if (engine) {
            engine->setWindowParameters({
                .title = settings.title,
                .requested_size = settings.window_size,
            });
        }
    }

    void Application::initDefaultWorld(establish::Realm& world) {
        prepareEngineWindow();
        establish::Module::RootId root;
        state->createDefaultState(world, root);
        if (not world.result().good())
            throw std::runtime_error("app: initDefaultWorld failed");
    }

    void Application::loadWorld(establish::Realm& world, filepath from) {
        prepareEngineWindow();
        worldFrom = std::move(from);
        state->loadPastState(world);
        if (not world.result().good())
            throw std::runtime_error("app: loadWorld failed");
    }

    int Application::run(establish::Realm& world) {
        while (engine and not engine->shouldClose(world))
            engine->frame(world);

        if (engine)
            engine->shutdown(world);
        return 0;
    }

}
