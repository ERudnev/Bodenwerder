#include <Raidenmamare/viewport.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>

namespace rmmr {
    struct Viewport_private : Viewport::Operations {
        static auto window(Reading world, Id id, resources::Manager manager) -> GLFWwindow* {
            const auto& quantum = ops::particle::get<Viewport>(world, id);
            return Device::Operations::provide(world, quantum.device, manager);
        }
    };

    void Viewport::Operations::activate(Reading world, Id id, resources::Manager manager) {
        const auto& quantum = ops::particle::get<Viewport>(world, id);
        glfwMakeContextCurrent(Viewport_private::window(world, id, manager));

        const int x = static_cast<int>(quantum.origin.x);
        const int y = static_cast<int>(quantum.origin.y);
        const int width = std::max(static_cast<int>(quantum.size.x), 1);
        const int height = std::max(static_cast<int>(quantum.size.y), 1);
        glViewport(x, y, width, height);
    }

    void Viewport::Operations::clear(Reading world, Id id, resources::Manager manager) {
        const auto& quantum = ops::particle::get<Viewport>(world, id);
        glfwMakeContextCurrent(Viewport_private::window(world, id, manager));
        glClearColor(
            quantum.clear_color.x,
            quantum.clear_color.y,
            quantum.clear_color.z,
            quantum.clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    const Invariants Viewport::invariants{
        .structural = {},
        .logical = {},
    };
}
