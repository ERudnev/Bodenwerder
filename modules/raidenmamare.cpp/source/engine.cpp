#include <rmmr/engine.h>

#include <base/logging.h>
#include <base/maybe.h>
#include <stdexcept>
#include <vector>

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
#include <rmmr/system/core.q1.h>
#include <rmmr/system/imgui.q1.h>
#include <rmmr/system/interface.q1.h>
#include <rmmr/system/viewport.q1.h>

#include "renderer/renderer.h"
#include "ui/overlay.h"

#include <GLFW/glfw3.h>

namespace {
    using namespace fqsm::api;
    using namespace rmmr;

    Schema generateInternalEngineSchema_static() {
        return ask::schema::merge({
            ask::schema::aspect<system::Core>(),
            ask::schema::aspect<system::Device>(),
            ask::schema::aspect<system::ImGuiHost>(),
            ask::schema::aspect<system::Window>(),
            ask::schema::aspect<system::Viewport>(),
            ask::schema::aspect<system::Viewport_group>(),
            ask::schema::aspect<resource::Manager>(),
            ask::schema::aspect<resource::Unit>(),
            ask::schema::aspect<resource::Unit_group>(),
            ask::schema::aspect<resource::DeviceRuntimes>(),
            ask::schema::aspect<resource::Runtime_group>(),
            ask::schema::aspect<resource::ShaderRuntime_group>(),
            ask::schema::aspect<resource::MaterialRuntime_group>(),
            ask::schema::aspect<resource::ShadowRuntime_group>(),
            ask::schema::aspect<resource::GeometryRuntime_group>(),
            ask::schema::aspect<resource::Assets>(),
            ask::schema::aspect<resource::Runtimes>(),
            ask::schema::aspect<resource::texture::Asset>(),
            ask::schema::aspect<resource::texture::Loader>(),
            ask::schema::aspect<resource::texture::Generator>(),
            ask::schema::aspect<resource::texture::Runtime>(),
            ask::schema::aspect<resource::shader::Asset>(),
            ask::schema::aspect<resource::shader::Loader>(),
            ask::schema::aspect<resource::shader::Runtime>(),
            ask::schema::aspect<resource::material::Asset>(),
            ask::schema::aspect<resource::material::Composer>(),
            ask::schema::aspect<resource::material::Runtime>(),
            ask::schema::aspect<resource::shadow::Asset>(),
            ask::schema::aspect<resource::shadow::Allocator>(),
            ask::schema::aspect<resource::shadow::Runtime>(),
            ask::schema::aspect<resource::geometry::Asset>(),
            ask::schema::aspect<resource::geometry::Loader>(),
            ask::schema::aspect<resource::geometry::Generator>(),
            ask::schema::aspect<resource::geometry::Runtime>(),
            ask::schema::aspect<controller::Camera>(),
            ask::schema::aspect<scene::Root>(),
            ask::schema::aspect<scene::Node>(),
            ask::schema::aspect<scene::Node_group>(),
            ask::schema::aspect<scene::Camera>(),
            ask::schema::aspect<scene::Camera_group>(),
            ask::schema::aspect<scene::Light>(),
            ask::schema::aspect<scene::Light_group>(),
            ask::schema::aspect<scene::PrimitiveActor>(),
            ask::schema::aspect<scene::Grid>(),
        });
    }

    Schema generateInterfaceEngineSchema_static() {
        return ask::schema::merge({
            //ask::schema::aspect<system::Interface>(),
        });
    }
}

namespace rmmr {
    using namespace fqsm::api;

    struct Engine::State {
        establish::Realm main;
        maybe<system::Core::Id> core;
        maybe<system::Device::Id> device;
        maybe<system::Viewport::Id> viewport;
        maybe<scene::Root::Id> scene;
        maybe<scene::Camera::Id> scene_camera;

        struct {
            struct {
                std::vector<resource::texture::Asset::Id> debug;
            } texture;
            struct {
                maybe<resource::material::Asset::Id> ambient;
                maybe<resource::material::Asset::Id> lit;
                std::vector<resource::material::Asset::Id> debugLitTextured;
                maybe<resource::material::Asset::Id> grid;
                maybe<resource::material::Asset::Id> shadowDepth;
            } material;
            struct {
                maybe<resource::geometry::Asset::Id> triangle;
                maybe<resource::geometry::Asset::Id> kube;
                maybe<resource::geometry::Asset::Id> bagel;
                maybe<resource::geometry::Asset::Id> grid;
            } primitive;

            maybe<resource::shadow::Asset::Id> shadow;
        } resources;
        Renderer renderer;
    };

    Engine::Engine(StartupParameters params)
        : interface(generateInterfaceEngineSchema_static())
        , state(std::make_shared<State>(State{
            .main = establish::Realm{generateInternalEngineSchema_static()},
        }))
    {
        base::message("rmmr: creating system core...");
        state->core = with<system::Interface>::create(state->main, std::move(params.assets_root), system::Core::GLVer{
            .major = params.context_major,
            .minor = params.context_minor,
        });

        base::message("rmmr: creating device and window...");
        state->device = with<system::Interface>::addDeviceAndWindow(state->main, *state->core, std::move(params.title), params.requested_size);
        prepareResources();
        prepareRenderTargets();
        createViewport(with<system::Window>::framebufferSize(state->main, *state->device));
        createScene();

        if (not state->main.result().good()) {
            throw std::runtime_error("Engine bootstrap failed");
        }
    }

    Engine::~Engine() noexcept {
        shutdown();
    }

    int Engine::run_render_demo() {
        auto& main = state->main;
        const auto& device = state->device;
        const auto& viewport = state->viewport;

        GLFWwindow* const handle = with<system::Device>::get(main, device).handle;

        while (not glfwWindowShouldClose(handle)) {
            with<system::Device>::poll_events(main);

            if (glfwGetKey(handle, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(handle, true);
            }

            with<system::Window>::onFrameAdvanced(main, device);
            if (state->scene_camera) {
                with<controller::Camera>::update(main, state->scene_camera, device);
            }

            with<system::Viewport>::syncExtent(main, viewport);
            with<system::Viewport>::activate(main, viewport);
            with<system::Viewport>::clear(main, viewport);

            /* natural perfomance test, keep this as comment please
            for (int xx = 0; xx < 100; ++xx)
                with<system::Viewport>::modify(main, viewport)->clear_color.r = 0;
            */

            if (state->scene && state->scene_camera) {
                with<system::ImGuiHost>::newFrame(main, device);
                const ui::FrameContext ui_frame{
                    .world = main,
                    .window = *device,
                    .scene = *state->scene,
                    .camera = *state->scene_camera,
                };
                ui::draw_camera(ui_frame);
                ui::draw_materials(ui_frame);
                state->renderer.render(Renderer::FrameContext{
                    .world = main,
                    .viewport = *viewport,
                    .window = *device,
                    .scene = *state->scene,
                    .camera = *state->scene_camera,
                });
                with<system::ImGuiHost>::render(main, device);
            }

            with<system::Window>::present(main, device);
        }

        return 0;
    }

    void Engine::prepareResources() {
        auto& main = state->main;
        const auto assets = *state->core;
        const auto device = *state->device;
        auto& resources = state->resources;

        using resource::Assets;
        using resource::Unit;
        using resource::geometry::Generator;

        base::message("rmmr: loading resources...");

        resources.texture.debug.push_back(with<Assets>::add_texture_loader(
            main,
            assets,
            Unit::Quantum{.manager = assets, .name = "debug01", .library = "rmmr"},
            resource::texture::Asset::Quantum{},
            resource::texture::Loader::Quantum{.file = "textures/debug01.jpg"}));
        resources.texture.debug.push_back(with<Assets>::add_texture_loader(
            main,
            assets,
            Unit::Quantum{.manager = assets, .name = "debug02", .library = "rmmr"},
            resource::texture::Asset::Quantum{},
            resource::texture::Loader::Quantum{.file = "textures/debug02.jpg"}));
        resources.texture.debug.push_back(with<Assets>::add_texture_loader(
            main,
            assets,
            Unit::Quantum{.manager = assets, .name = "debug03", .library = "rmmr"},
            resource::texture::Asset::Quantum{},
            resource::texture::Loader::Quantum{.file = "textures/debug03.jpg"}));
        resources.texture.debug.push_back(with<Assets>::add_texture_loader(
            main,
            assets,
            Unit::Quantum{.manager = assets, .name = "debug04", .library = "rmmr"},
            resource::texture::Asset::Quantum{},
            resource::texture::Loader::Quantum{.file = "textures/debug04.jpg"}));

        const auto ambient_shader = with<Assets>::add_shader_loader(main, assets, Unit::Quantum{.manager = assets, .name = "ambient_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/ambient.vert.glsl", .fragment = "shaders/ambient.frag.glsl"});
        const auto lit_shader = with<Assets>::add_shader_loader(main, assets, Unit::Quantum{.manager = assets, .name = "lit_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/lit.vert.glsl", .fragment = "shaders/lit.frag.glsl"});
        const auto lit_textured_shader = with<Assets>::add_shader_loader(main, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTextured.frag.glsl"});
        const auto grid_shader = with<Assets>::add_shader_loader(main, assets, Unit::Quantum{.manager = assets, .name = "grid_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/Grid.vert.glsl", .fragment = "shaders/Grid.frag.glsl"});
        const auto shadow_depth_shader = with<Assets>::add_shader_loader(main, assets, Unit::Quantum{.manager = assets, .name = "shadow_depth_shader", .library = "rmmr"}, resource::shader::Asset::Quantum{}, resource::shader::Loader::Quantum{.vertex = "shaders/shadowDepth.vert.glsl", .fragment = "shaders/shadowDepth.frag.glsl"});

        resources.material.ambient = with<Assets>::add_material(main, assets, Unit::Quantum{.manager = assets, .name = "ambient_material", .library = "rmmr"}, resource::builders::material::MaterialPresets::ambient(ambient_shader), resource::material::Composer::Quantum{});
        resources.material.lit = with<Assets>::add_material(main, assets, Unit::Quantum{.manager = assets, .name = "lit_material", .library = "rmmr"}, resource::builders::material::MaterialPresets::lit(lit_shader), resource::material::Composer::Quantum{});
        resources.material.debugLitTextured.push_back(with<Assets>::add_material(main, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug01", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, resources.texture.debug[0]), resource::material::Composer::Quantum{}));
        resources.material.debugLitTextured.push_back(with<Assets>::add_material(main, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug02", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, resources.texture.debug[1]), resource::material::Composer::Quantum{}));
        resources.material.debugLitTextured.push_back(with<Assets>::add_material(main, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug03", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, resources.texture.debug[2]), resource::material::Composer::Quantum{}));
        resources.material.debugLitTextured.push_back(with<Assets>::add_material(main, assets, Unit::Quantum{.manager = assets, .name = "lit_textured_debug04", .library = "rmmr"}, resource::builders::material::MaterialPresets::litTextured(lit_textured_shader, resources.texture.debug[3]), resource::material::Composer::Quantum{}));
        resources.material.grid = with<Assets>::add_material(main, assets, Unit::Quantum{.manager = assets, .name = "grid_material", .library = "rmmr"}, resource::builders::material::MaterialPresets::grid(grid_shader), resource::material::Composer::Quantum{});
        resources.material.shadowDepth = with<Assets>::add_material(main, assets, Unit::Quantum{.manager = assets, .name = "shadow_depth_material", .library = "rmmr"}, resource::builders::material::MaterialPresets::shadowDepth(shadow_depth_shader), resource::material::Composer::Quantum{});
        scene::PrimitiveActor::Actions::modify_global(main)->shadowMaterial = *resources.material.shadowDepth;

        resources.primitive.triangle = with<Assets>::add_geometry_generator(main, assets, Unit::Quantum{.manager = assets, .name = "triangle", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::triangle});
        resources.primitive.kube = with<Assets>::add_geometry_generator(main, assets, Unit::Quantum{.manager = assets, .name = "kube", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::kube});
        resources.primitive.bagel = with<Assets>::add_geometry_generator(main, assets, Unit::Quantum{.manager = assets, .name = "bagel", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::bagel});
        resources.primitive.grid = with<Assets>::add_geometry_generator(main, assets, Unit::Quantum{.manager = assets, .name = "grid", .library = "rmmr"}, resource::geometry::Asset::Quantum{}, Generator::Quantum{.type = Generator::Type::gridPlane});

        resources.shadow = with<Assets>::add_shadow_allocator(main, assets, Unit::Quantum{.manager = assets, .name = "main_shadow", .library = "rmmr"}, resource::shadow::Asset::Quantum{}, resource::shadow::Allocator::Quantum{.size = index2{1024, 1024}});

        with<resource::Runtimes>::materialize(main, device, assets);

        base::message("rmmr: resources loaded");
    }

    void Engine::prepareRenderTargets() {
        // Shadow Asset lives in State.resources.shadow and is materialized with the catalog.
    }

    void Engine::createScene() {
        auto& main = state->main;
        const auto& resources = state->resources;

        const auto root = with<scene::Interface>::createScene(main);

        constexpr int grid_extent = 10;
        constexpr float cube_edge = 1.0f;
        constexpr float spacing = cube_edge * 1.5f;
        const float center_offset = (static_cast<float>(grid_extent) - 1.0f) * 0.5f;
        const float cluster_lift = center_offset * spacing + cube_edge * 0.5f;

        main.branch([&](fqsm::Writing context) {
            for (int z = 0; z < grid_extent; ++z) {
                for (int y = 0; y < grid_extent; ++y) {
                    for (int x = 0; x < grid_extent; ++x) {
                        const int cell = x + y + z;
                        const Pos pos{(static_cast<float>(x) - center_offset) * spacing, (static_cast<float>(y) - center_offset) * spacing + cluster_lift, (static_cast<float>(z) - center_offset) * spacing};
                        with<scene::Interface>::createPrimitiveActor(context, root,
                            Locator{.pos = pos, .euler = HPB{-22.5f + 45.0f * static_cast<float>(x), -15.0f + 30.0f * static_cast<float>(y), -12.0f + 24.0f * static_cast<float>(z)}},
                            scene::PrimitiveActor::Quantum{
                                .geometry = (cell % 7 == 0) ? *resources.primitive.bagel : *resources.primitive.kube,
                                .material = resources.material.debugLitTextured[cell % 4],
                                .albedo = RGB{0.3f + 0.6f * static_cast<float>(x) / static_cast<float>(grid_extent - 1), 0.3f + 0.6f * static_cast<float>(y) / static_cast<float>(grid_extent - 1), 0.3f + 0.6f * static_cast<float>(z) / static_cast<float>(grid_extent - 1)},
                            });
                    }
                }
            }
        });

        with<scene::Interface>::createGrid(main, root, Locator{.pos = Pos{0.0f, 0.0f, 0.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}}, scene::Grid::Quantum{.geometry = *resources.primitive.grid, .material = *resources.material.grid, .opacity = 1.0f});

        const auto scene_camera = with<scene::Interface>::createCamera(main, root, Locator{.pos = Pos{10.5f, 10.0f, 14.0f}, .euler = HPB{-18.0f, -36.0f, 0.0f}}, scene::Camera::Quantum{.fov_y = 1.04719755f, .z_near = 0.1f, .z_far = 100.0f});
        with<controller::Camera>::create(main, scene_camera);
        state->scene_camera = scene_camera;
        with<scene::Interface>::createLight(main, root, Locator{.pos = Pos{9.5f, 19.0f, 7.5f}, .euler = HPB{0.0f, 0.0f, 0.0f}}, scene::Light::Quantum{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 30.0f, .shadow = resources.shadow});
        state->scene = root;
    }

    void Engine::createViewport(index2 size, index2 origin) {
        state->viewport = with<system::Viewport_group>::addElement(state->main, *state->device, system::Viewport::Quantum{.origin = origin, .size = size, .clear_color = vec4{0.2f, 0.3f, 0.3f, 1.0f}});
    }

    void Engine::shutdown() noexcept {
        base::message("rmmr teardown: Engine shutdown begin");
        // Device quanta may die inside fQSM earlier than native GLFW handles should.
        // Preserve native windows here so retrospective release workers can finish first.
        std::vector<GLFWwindow*> preserved_handles;
        for (const auto entry : state->main->aspect<system::Device>().items()) {
            if (entry.value.handle) {
                preserved_handles.push_back(entry.value.handle);
            }
        }
        ask::temp_sugar::drop_reference<system::Viewport>(state->main, state->viewport);
        with<system::Interface>::shutdown(state->main);
        for (GLFWwindow* handle : preserved_handles) {
            glfwDestroyWindow(handle);
        }
        glfwTerminate();
        base::message("rmmr teardown: Engine shutdown done");
    }
}
