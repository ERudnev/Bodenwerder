#include <Raidenmamare/engine.h>
#include <Raidenmamare/viewport.q1.h>

#include <GLFW/glfw3.h>

#include <base/logging.h>

#include <stdexcept>
#include <utility>

namespace {
    using namespace fqsm::api;
    using namespace rmmr;

    Schema generateInternalEngineSchema_static() {
        return ask::schema::merge({
            ask::schema::aspect<Window>(),
            ask::schema::aspect<Device>(),
            ask::schema::aspect<Viewport>(),
        });
    }

    Schema generateInterfaceEngineSchema_static() {
        return ask::schema::merge({
            ask::schema::aspect<Window>(),
        });
    }
}

namespace rmmr {
    using namespace fqsm::api;

    struct Engine::State {
        establish::Realm main;
        maybe<Device::Id> device;
        maybe<Viewport::Id> viewport;
    };

    Engine::Engine(StartupParameters params)
        : interface(generateInterfaceEngineSchema_static())
        , state(std::make_shared<State>(State{
            .main = establish::Realm{generateInternalEngineSchema_static()},
        }))
    {
        base::message("rmmr: creating device...");
        state->device = with<Device>::create(state->main, Device::Quantum{
            .assets_root = std::move(params.assets_root),
            .context_major = params.context_major,
            .context_minor = params.context_minor,
            .window = Window::Id{0},
        });
        with<Device>::init(state->main, state->device, std::move(params.title), params.size);
        createViewport(params.size);

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

        GLFWwindow* const window = with<Window>::get(main, with<Device>::get(main, device).window).handle;
        if (not window) throw std::runtime_error("Engine::run_render_demo: window is not initialized");

        while (not glfwWindowShouldClose(window)) {
            with<Device>::poll_events(main, device);

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);

            with<Viewport>::activate(main, viewport);
            with<Viewport>::clear(main, viewport);
            with<Device>::present(main, device);
        }

        return 0;
    }

    void Engine::prepareResources() {
        _INCOMPLETE_;
    }

    void Engine::createScene() {
        _INCOMPLETE_;
    }

    void Engine::createViewport(index2 size, index2 origin) {
        state->viewport = with<Viewport>::create(state->main, Viewport::Quantum{
            .device = state->device,
            .origin = origin,
            .size = size,
            .clear_color = vec4{0.2f, 0.3f, 0.3f, 1.0f},
        });
    }

    void Engine::shutdown() noexcept {
        ask::temp_sugar::drop_reference<Viewport>(state->main, state->viewport);
        ask::temp_sugar::drop_reference<Device>(state->main, state->device);
    }
}
