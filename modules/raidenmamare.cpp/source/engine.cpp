#include <Raidenmamare/engine.h>
#include <Raidenmamare/materials/core.q1.h>
#include <Raidenmamare/scene/core.q1.h>
#include <Raidenmamare/viewport.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <base/logging.h>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include "materials/materialGenerator.h"

namespace {
    using namespace fqsm::api;
    using namespace rmmr;

    Schema generateInternalEngineSchema_static() {
        return ask::schema::merge({
            ask::schema::aspect<Application>(),
            ask::schema::aspect<Window>(),
            ask::schema::aspect<material::Program>(),
            ask::schema::aspect<material::Program_group>(),
            ask::schema::aspect<material::Core>(),
            ask::schema::aspect<Viewport>(),
            ask::schema::aspect<scene::Core>(),
            ask::schema::aspect<scene::Node>(),
            ask::schema::aspect<scene::Node_group>(),
            ask::schema::aspect<scene::Camera>(),
            ask::schema::aspect<scene::Camera_group>(),
            ask::schema::aspect<scene::Light>(),
            ask::schema::aspect<scene::Light_group>(),
        });
    }

    Schema generateInterfaceEngineSchema_static() {
        return ask::schema::merge({
            ask::schema::aspect<Window>(),
        });
    }
}


namespace rmmr::device {
    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto create_handle(
            const Application::Global& application,
            const decltype(Window::Quantum::title)& title,
            const decltype(Window::Quantum::size)& size) -> Window::Handle {
            const int width = std::max(static_cast<int>(size.x), 1);
            const int height = std::max(static_cast<int>(size.y), 1);
            const int context_major = std::max(static_cast<int>(application.context_major), 1);
            const int context_minor = std::max(static_cast<int>(application.context_minor), 0);
            const char* window_title = title.empty() ? "Raidenmamare" : title.c_str();

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, context_major);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, context_minor);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            GLFWwindow* window = glfwCreateWindow(width, height, window_title, nullptr, nullptr);
            if (!window) {
                throw std::runtime_error("device::openWindow: glfwCreateWindow() failed");
            }

            glfwSetWindowPos(window, 1000, 100);
            glfwMakeContextCurrent(window);
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                glfwDestroyWindow(window);
                throw std::runtime_error("device::openWindow: glewInit() failed");
            }

            glEnable(GL_DEPTH_TEST);

            glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int framebuffer_width, int framebuffer_height) {
                glViewport(0, 0, framebuffer_width, framebuffer_height);
            });

            return window;
        }

    } // namespace

    void start(Writing) {
        if (!glfwInit()) {
            throw std::runtime_error("device::start: glfwInit() failed");
        }
    }

    auto openWindow(
        Writing context,
        decltype(Window::Quantum::title) title,
        decltype(Window::Quantum::size) size) -> Window::Id {
        const Application::Global& application = with<Application>::get_global(context);
        const Window::Handle handle = create_handle(application, title, size);
        return with<Window>::create(context, Window::Quantum{
            .title = std::move(title),
            .size = size,
            .handle = handle,
        });
    }

    void shutdown(Writing context) {
        std::vector<Window::Id> windowIds;
        for (const auto entry : context->aspect<Window>().items()) {
            windowIds.push_back(entry.id);
        }
        for (Window::Id windowId : windowIds) {
            with<Window>::remove(context, windowId);
        }
        glfwTerminate();
    }
}

namespace rmmr {
    using namespace fqsm::api;

    struct Engine::State {
        establish::Realm main;
        maybe<Window::Id> window;
        maybe<Viewport::Id> viewport;
        maybe<scene::Core::Id> scene;
        struct {
            maybe<material::Core::Id> materialAmbient;
            maybe<material::Core::Id> materialLit;
            maybe<material::Core::Id> materialGrid;
        } resources;
    };

    Engine::Engine(StartupParameters params)
        : interface(generateInterfaceEngineSchema_static())
        , state(std::make_shared<State>(State{
            .main = establish::Realm{generateInternalEngineSchema_static()},
        }))
    {
        base::message("rmmr: starting device...");
        device::start(state->main);

        {
            auto global = with<Application>::modify_global(state->main);
            global.assets_root = std::move(params.assets_root);
            global.context_major = params.context_major;
            global.context_minor = params.context_minor;
        }
        state->window = device::openWindow(state->main, std::move(params.title), params.size);
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

        GLFWwindow* const handle = with<Window>::get(main, window).handle;
        if (not handle) throw std::runtime_error("Engine::run_render_demo: window is not initialized");

        while (not glfwWindowShouldClose(handle)) {
            with<Application>::poll_events(main);

            if (glfwGetKey(handle, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(handle, true);

            with<Viewport>::activate(main, viewport);
            with<Viewport>::clear(main, viewport);
            with<Window>::present(main, window);
        }

        return 0;
    }

    void Engine::prepareResources() {
        auto& main = state->main;
        const auto& window = state->window;

        base::message("rmmr: loading material resources...");
        with<material::Program_group>::extend(main, window);

        state->resources.materialAmbient = material::MaterialGenerator::ambient(main, window);
        state->resources.materialLit = material::MaterialGenerator::lit(main, window);
        state->resources.materialGrid = material::MaterialGenerator::grid(main, window);

        base::message("rmmr: material resources loaded");
    }

    void Engine::createScene() {
        auto& main = state->main;
        const auto core = with<scene::Interface>::createScene(main);
        with<scene::Interface>::createCamera(main, core, Locator{
            .pos = Pos{0.0f, 1.3f, 4.0f},
            .euler = HPB{0.0f, -12.5f, 0.0f},
        }, scene::Camera::Quantum{
            .fov_y = 1.04719755f,
            .z_near = 0.1f,
            .z_far = 100.0f,
        });
        with<scene::Interface>::createLight(main, core, Locator{
            .pos = Pos{2.0f, 2.0f, 2.0f},
            .euler = HPB{0.0f, 0.0f, 0.0f},
        }, scene::Light::Quantum{
            .color = RGB{1.0f, 1.0f, 1.0f},
            .intensity = 5.0f,
            .range = 10.0f,
        });
        state->scene = core;
    }

    void Engine::createViewport(index2 size, index2 origin) {
        state->viewport = with<Viewport>::create(state->main, Viewport::Quantum{
            .window = state->window,
            .origin = origin,
            .size = size,
            .clear_color = vec4{0.2f, 0.3f, 0.3f, 1.0f},
        });
    }

    void Engine::shutdown() noexcept {
        ask::temp_sugar::drop_reference<Viewport>(state->main, state->viewport);
        device::shutdown(state->main);
    }
}
