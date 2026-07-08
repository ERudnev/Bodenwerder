#include <Raidenmamare/viewport.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>

namespace rmmr {
    struct Viewport::Internals : Viewport::DefaultInternals {
        static auto window_handle(Reading context, Window::Id window) -> Window::Handle {
            const Window::Handle handle = with<Window>::get(context, window).handle;
            if (!handle) {
                throw std::runtime_error("Viewport: window handle is not initialized");
            }
            return handle;
        }
    };

    void Viewport::Actions::activate(Reading context, Id id) {
        const auto& quantum = get(context, id);
        glfwMakeContextCurrent(Internals::window_handle(context, quantum.window));

        const int x = static_cast<int>(quantum.origin.x);
        const int y = static_cast<int>(quantum.origin.y);
        const int width = std::max(static_cast<int>(quantum.size.x), 1);
        const int height = std::max(static_cast<int>(quantum.size.y), 1);
        glViewport(x, y, width, height);
    }

    void Viewport::Actions::clear(Reading context, Id id) {
        const auto& quantum = get(context, id);
        glfwMakeContextCurrent(Internals::window_handle(context, quantum.window));
        glClearColor(
            quantum.clear_color.x,
            quantum.clear_color.y,
            quantum.clear_color.z,
            quantum.clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}
