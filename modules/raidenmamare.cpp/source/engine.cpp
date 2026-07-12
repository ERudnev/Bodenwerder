#include <rmmr/engine.h>

#include <rmmr/assets/geometry.q1.h>
#include <rmmr/assets/material.q1.h>
#include <rmmr/assets/shader.q1.h>
#include <rmmr/controller/camera.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/material.q1.h>
#include <rmmr/resources/shader.q1.h>
#include <rmmr/scene/actor.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/interface.q1.h>
#include <rmmr/resources/shadowMap.q1.h>
#include <rmmr/system/viewport.q1.h>

#include <GLFW/glfw3.h>

#include <base/logging.h>
#include <base/maybe.h>

#include <stdexcept>
#include <random>

#include "geometry/geometryGenerator.h"
#include "materials/materialGenerator.h"
#include "renderer/renderer.h"
#include "shadowMap/shadowMapGenerator.h"

namespace {
    using namespace fqsm::api;
    using namespace rmmr;

    Schema generateInternalEngineSchema_static() {
        return ask::schema::merge({
            ask::schema::aspect<system::Core>(),
            ask::schema::aspect<system::Device>(),
            ask::schema::aspect<system::Window>(),
            ask::schema::aspect<system::Viewport>(),
            ask::schema::aspect<system::Viewport_group>(),
            ask::schema::aspect<asset::Geometry>(),
            ask::schema::aspect<asset::Shader>(),
            ask::schema::aspect<asset::Material>(),
            ask::schema::aspect<resource::Geometry>(),
            ask::schema::aspect<resource::Geometry_group>(),
            ask::schema::aspect<resource::Shader>(),
            ask::schema::aspect<resource::Shader_group>(),
            ask::schema::aspect<resource::Material>(),
            ask::schema::aspect<resource::Material_group>(),
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
        maybe<system::Window::Id> window;
        maybe<system::Viewport::Id> viewport;
        maybe<scene::Root::Id> scene;
        maybe<scene::Camera::Id> scene_camera;
        struct {
            maybe<resource::Material::Id> materialAmbient;
            maybe<resource::Material::Id> materialLit;
            maybe<resource::Material::Id> materialGrid;
            maybe<resource::Material::Id> materialShadowDepth;
            maybe<resource::Geometry::Id> primitive;
            maybe<resource::Geometry::Id> primitiveKube;
            maybe<resource::Geometry::Id> primitiveGrid;
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
        with<system::Interface>::create(state->main, std::move(params.assets_root), system::Core::GLVer{
            .major = params.context_major,
            .minor = params.context_minor,
        });

        base::message("rmmr: creating window...");
        state->window = with<system::Interface>::createWindow(state->main, std::move(params.title), params.requested_size);
        prepareResources();
        prepareRenderTargets();
        createViewport(with<system::Window>::framebufferSize(state->main, *state->window));
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
        const auto& window = state->window;
        const auto& viewport = state->viewport;

        GLFWwindow* const handle = with<system::Device>::get(main, window).handle;

        while (not glfwWindowShouldClose(handle)) {
            with<system::Device>::poll_events(main);

            if (glfwGetKey(handle, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(handle, true);
            }

            with<system::Window>::onFrameAdvanced(main, window);
            if (state->scene_camera) {
                with<controller::Camera>::update(main, state->scene_camera, window);
            }

            with<system::Viewport>::syncExtent(main, viewport);
            with<system::Viewport>::activate(main, viewport);
            with<system::Viewport>::clear(main, viewport);

            if (state->scene && state->scene_camera && state->resources.shadowMap) {
                state->renderer.render(Renderer::FrameContext{
                    .world = main,
                    .viewport = *viewport,
                    .window = *window,
                    .scene = *state->scene,
                    .camera = *state->scene_camera,
                    .shadow_map = *state->resources.shadowMap,
                });
            }

            with<system::Window>::present(main, window);
        }

        return 0;
    }

    void Engine::prepareResources() {
        auto& main = state->main;
        const auto& window = state->window;

        base::message("rmmr: loading material resources...");
        state->resources.materialAmbient = material::MaterialGenerator::ambient(main, window);
        state->resources.materialLit = material::MaterialGenerator::lit(main, window);
        state->resources.materialGrid = material::MaterialGenerator::grid(main, window);
        state->resources.materialShadowDepth = material::MaterialGenerator::shadowDepth(main, window);
        scene::PrimitiveActor::Actions::modify_global(main)->shadowMaterial = *state->resources.materialShadowDepth;

        base::message("rmmr: loading geometry resources...");
        state->resources.primitive = geometry::GeometryGenerator::triangle(main, window);
        state->resources.primitiveKube = geometry::GeometryGenerator::kube(main, window);
        state->resources.primitiveGrid = geometry::GeometryGenerator::gridPlane(main, window);

        base::message("rmmr: material and geometry resources loaded");
    }

    void Engine::prepareRenderTargets() {
        auto& main = state->main;
        const auto& window = state->window;

        base::message("rmmr: creating render targets...");
        state->resources.shadowMap = shadow_map::ShadowMapGenerator::create(main, window, index2{1024, 1024});
        base::message("rmmr: render targets ready");
    }

    void Engine::createScene() {
        auto& main = state->main;
        const auto& resources = state->resources;

        const auto root = with<scene::Interface>::createScene(main);

        for (int i = 0; i < 5; ++i) {
            with<scene::Interface>::createPrimitiveActor(main, root, Locator{
                .pos = Pos{-1.4f + 0.7f * static_cast<float>(i), 0.5f, 0.0f},
                .euler = HPB{0.0f, 0.0f, 0.0f},
            }, scene::PrimitiveActor::Quantum{
                .geometry = *resources.primitive,
                .material = *resources.materialAmbient,
                .albedo = RGB{1.0f - 0.15f * static_cast<float>(i), 0.5f, 0.2f + 0.15f * static_cast<float>(i)},
            });
        }

        std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> heading_deg{-180.0f, 180.0f};
        std::uniform_real_distribution<float> pitch_deg{-45.0f, 45.0f};
        std::uniform_real_distribution<float> bank_deg{-45.0f, 45.0f};

        for (int i = 0; i < 4; ++i) {
            with<scene::Interface>::createPrimitiveActor(main, root, Locator{
                .pos = Pos{-1.05f + 0.7f * static_cast<float>(i), 0.2f, 0.0f},
                .euler = HPB{heading_deg(rng), pitch_deg(rng), bank_deg(rng)},
            }, scene::PrimitiveActor::Quantum{
                .geometry = *resources.primitiveKube,
                .material = *resources.materialLit,
                .albedo = RGB{0.2f + 0.2f * static_cast<float>(i), 0.45f, 1.0f - 0.2f * static_cast<float>(i)},
            });
        }

        with<scene::Interface>::createGrid(main, root, Locator{
            .pos = Pos{0.0f, 0.0f, 0.0f},
            .euler = HPB{0.0f, 0.0f, 0.0f},
        }, scene::Grid::Quantum{
            .geometry = *resources.primitiveGrid,
            .material = *resources.materialGrid,
            .opacity = 1.0f,
        });

        const auto scene_camera = with<scene::Interface>::createCamera(main, root, Locator{
            .pos = Pos{0.0f, 1.3f, 4.0f},
            .euler = HPB{0.0f, -12.5f, 0.0f},
        }, scene::Camera::Quantum{
            .fov_y = 1.04719755f,
            .z_near = 0.1f,
            .z_far = 100.0f,
        });
        with<controller::Camera>::create(main, scene_camera);
        state->scene_camera = scene_camera;
        with<scene::Interface>::createLight(main, root, Locator{
            .pos = Pos{2.0f, 2.0f, 2.0f},
            .euler = HPB{0.0f, 0.0f, 0.0f},
        }, scene::Light::Quantum{
            .color = RGB{1.0f, 1.0f, 1.0f},
            .intensity = 5.0f,
            .range = 10.0f,
        });
        state->scene = root;
    }

    void Engine::createViewport(index2 size, index2 origin) {
        state->viewport = with<system::Viewport_group>::addElement(state->main, *state->window, system::Viewport::Quantum{.origin = origin, .size = size, .clear_color = vec4{0.2f, 0.3f, 0.3f, 1.0f}});
    }

    void Engine::shutdown() noexcept {
        ask::temp_sugar::drop_reference<system::Viewport>(state->main, state->viewport);
        with<system::Interface>::shutdown(state->main);
    }
}
