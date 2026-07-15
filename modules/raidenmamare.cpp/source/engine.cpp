#include <rmmr/engine.h>

#include <rmmr/assets/geometry.q1.h>
#include <rmmr/assets/material.q1.h>
#include <rmmr/assets/shader.q1.h>
#include <rmmr/assets/texture.q1.h>
#include <rmmr/controller/camera.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/material.q1.h>
#include <rmmr/resources/shader.q1.h>
#include <rmmr/resources/texture.q1.h>
#include <rmmr/scene/actor.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/imgui.q1.h>
#include <rmmr/system/interface.q1.h>
#include <rmmr/resources/shadowMap.q1.h>
#include <rmmr/system/viewport.q1.h>

#include <GLFW/glfw3.h>

#include <base/logging.h>
#include <base/maybe.h>

#include <stdexcept>
#include <vector>

#include "geometry/geometryGenerator.h"
#include "materials/materialGenerator.h"
#include "renderer/renderer.h"
#include "ui/overlay.h"
#include "shadowMap/shadowMapGenerator.h"

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
            ask::schema::aspect<asset::Geometry>(),
            ask::schema::aspect<asset::Shader>(),
            ask::schema::aspect<asset::Material>(),
            ask::schema::aspect<asset::Texture>(),
            ask::schema::aspect<resource::Geometry>(),
            ask::schema::aspect<resource::Geometry_group>(),
            ask::schema::aspect<resource::Shader>(),
            ask::schema::aspect<resource::Shader_group>(),
            ask::schema::aspect<resource::Material>(),
            ask::schema::aspect<resource::Material_group>(),
            ask::schema::aspect<resource::Texture>(),
            ask::schema::aspect<resource::Texture_group>(),
            ask::schema::aspect<resource::ShadowMap>(),
            ask::schema::aspect<resource::ShadowMap_group>(),
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
                maybe<resource::Texture::Id> debug;
            } texture;
            struct {
                maybe<resource::Material::Id> ambient;
                maybe<resource::Material::Id> lit;
                maybe<resource::Material::Id> litTextured;
                maybe<resource::Material::Id> grid;
                maybe<resource::Material::Id> shadowDepth;
            } material;
            struct {
                maybe<resource::Geometry::Id> triangle;
                maybe<resource::Geometry::Id> kube;
                maybe<resource::Geometry::Id> grid;
            } primitive;
            maybe<resource::ShadowMap::Id> shadowMap;
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

            if (state->scene && state->scene_camera && state->resources.shadowMap) {
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
                    .shadow_map = *state->resources.shadowMap,
                });
                with<system::ImGuiHost>::render(main, device);
            }

            with<system::Window>::present(main, device);
        }

        return 0;
    }

    void Engine::prepareResources() {
        auto& main = state->main;
        const auto& device = state->device;
        auto& resources = state->resources;

        const auto debug_texture = with<asset::Texture>::create(main, asset::Texture::Quantum{
            .name = "debug01.jpg",
            .library = "rmmr",
        });
        resources.texture.debug = asset::Texture::Actions::compile(main, debug_texture, device);

        base::message("rmmr: loading material resources...");
        resources.material.ambient = material::MaterialGenerator::ambient(main, device);
        resources.material.lit = material::MaterialGenerator::lit(main, device);
        resources.material.litTextured = material::MaterialGenerator::litTextured(main, device, debug_texture);
        resources.material.grid = material::MaterialGenerator::grid(main, device);
        resources.material.shadowDepth = material::MaterialGenerator::shadowDepth(main, device);
        scene::PrimitiveActor::Actions::modify_global(main)->shadowMaterial = *resources.material.shadowDepth;

        base::message("rmmr: loading geometry resources...");
        resources.primitive.triangle = geometry::GeometryGenerator::triangle(main, device);
        resources.primitive.kube = geometry::GeometryGenerator::kube(main, device);
        resources.primitive.grid = geometry::GeometryGenerator::gridPlane(main, device);

        base::message("rmmr: material and geometry resources loaded");
    }

    void Engine::prepareRenderTargets() {
        auto& main = state->main;
        const auto& device = state->device;

        base::message("rmmr: creating render targets...");
        state->resources.shadowMap = shadow_map::ShadowMapGenerator::create(main, device, index2{1024, 1024});
        base::message("rmmr: render targets ready");
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
                        const Pos pos{
                            (static_cast<float>(x) - center_offset) * spacing,
                            (static_cast<float>(y) - center_offset) * spacing + cluster_lift,
                            (static_cast<float>(z) - center_offset) * spacing,
                        };

                        with<scene::Interface>::createPrimitiveActor(context, root, Locator{
                            .pos = pos,
                            .euler = HPB{
                                -22.5f + 45.0f * static_cast<float>(x),
                                -15.0f + 30.0f * static_cast<float>(y),
                                -12.0f + 24.0f * static_cast<float>(z),
                            },
                        }, scene::PrimitiveActor::Quantum{
                            .geometry = *resources.primitive.kube,
                            .material = *resources.material.litTextured,
                            .albedo = RGB{
                                0.3f + 0.6f * static_cast<float>(x) / static_cast<float>(grid_extent - 1),
                                0.3f + 0.6f * static_cast<float>(y) / static_cast<float>(grid_extent - 1),
                                0.3f + 0.6f * static_cast<float>(z) / static_cast<float>(grid_extent - 1),
                            },
                        });
                    }
                }
            }
        });

        with<scene::Interface>::createGrid(main, root, Locator{
            .pos = Pos{0.0f, 0.0f, 0.0f},
            .euler = HPB{0.0f, 0.0f, 0.0f},
        }, scene::Grid::Quantum{
            .geometry = *resources.primitive.grid,
            .material = *resources.material.grid,
            .opacity = 1.0f,
        });

        const auto scene_camera = with<scene::Interface>::createCamera(main, root, Locator{
            .pos = Pos{10.5f, 10.0f, 14.0f},
            .euler = HPB{-18.0f, -36.0f, 0.0f},
        }, scene::Camera::Quantum{
            .fov_y = 1.04719755f,
            .z_near = 0.1f,
            .z_far = 100.0f,
        });
        with<controller::Camera>::create(main, scene_camera);
        state->scene_camera = scene_camera;
        with<scene::Interface>::createLight(main, root, Locator{
            .pos = Pos{9.5f, 19.0f, 7.5f},
            .euler = HPB{0.0f, 0.0f, 0.0f},
        }, scene::Light::Quantum{
            .color = RGB{1.0f, 0.94f, 0.86f},
            .intensity = 7.0f,
            .range = 30.0f,
        });
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
