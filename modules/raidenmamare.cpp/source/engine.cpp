#include <Raidenmamare/engine.h>

#include <Raidenmamare/assets/geometry.q1.h>
#include <Raidenmamare/assets/material.q1.h>
#include <Raidenmamare/assets/shader.q1.h>
#include <Raidenmamare/controller/camera.q1.h>
#include <Raidenmamare/resources/geometry.q1.h>
#include <Raidenmamare/resources/material.q1.h>
#include <Raidenmamare/resources/shader.q1.h>
#include <Raidenmamare/scene/root.q1.h>
#include <Raidenmamare/system/core.q1.h>
#include <Raidenmamare/system/interface.q1.h>
#include <Raidenmamare/system/viewport.q1.h>

#include <GLFW/glfw3.h>

#include <base/logging.h>
#include <base/maybe.h>

#include <stdexcept>

#include "materials/materialGenerator.h"

namespace {
    using namespace fqsm::api;
    using namespace rmmr;

    Schema generateInternalEngineSchema_static() {
        return ask::schema::merge({
            ask::schema::aspect<system::Core>(),
            ask::schema::aspect<system::Device>(),
            ask::schema::aspect<system::Device_group>(),
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
            ask::schema::aspect<controller::Camera>(),
            ask::schema::aspect<scene::Root>(),
            ask::schema::aspect<scene::Node>(),
            ask::schema::aspect<scene::Node_group>(),
            ask::schema::aspect<scene::Camera>(),
            ask::schema::aspect<scene::Camera_group>(),
            ask::schema::aspect<scene::Light>(),
            ask::schema::aspect<scene::Light_group>(),
            ask::schema::aspect<scene::PrimitiveActor>(),
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
        } resources;
    };

    Engine::Engine(StartupParameters params)
        : interface(generateInterfaceEngineSchema_static())
        , state(std::make_shared<State>(State{
            .main = establish::Realm{generateInternalEngineSchema_static()},
        }))
    {
        base::message("rmmr: creating window...");

        {
            auto global = with<system::Device>::modify_global(state->main);
            global->assets_root = std::move(params.assets_root);
            global->context_major = params.context_major;
            global->context_minor = params.context_minor;
        }

        state->window = with<system::Interface>::createWindow(state->main, std::move(params.title), params.size);
        prepareResources();
        createViewport(params.size);
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

            with<system::Viewport>::activate(main, viewport);
            with<system::Viewport>::clear(main, viewport);
            with<system::Window>::present(main, window);
        }

        return 0;
    }

    void Engine::prepareResources() {
        auto& main = state->main;
        const auto& window = state->window;

        base::message("rmmr: loading material resources...");
        with<resource::Shader_group>::extend(main, window);
        with<resource::Material_group>::extend(main, window);
        with<resource::Geometry_group>::extend(main, window);

        state->resources.materialAmbient = material::MaterialGenerator::ambient(main, window);
        state->resources.materialLit = material::MaterialGenerator::lit(main, window);
        state->resources.materialGrid = material::MaterialGenerator::grid(main, window);

        base::message("rmmr: material resources loaded");
    }

    void Engine::createScene() {
        auto& main = state->main;
        const auto root = with<scene::Interface>::createScene(main);
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
