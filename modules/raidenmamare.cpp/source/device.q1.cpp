#include <Raidenmamare/device.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>

namespace rmmr {
    struct Device_private : Device::Operations {
        using Passport = Device::Materializer::Passport;

        static auto init_window(const Passport& passport) -> GLFWwindow* {
            if (!glfwInit()) {
                throw std::runtime_error("Device::Materializer::materialize: glfwInit() failed");
            }

            const int width = std::max(static_cast<int>(passport.size.x), 1);
            const int height = std::max(static_cast<int>(passport.size.y), 1);
            const int context_major = std::max(static_cast<int>(passport.context_major), 1);
            const int context_minor = std::max(static_cast<int>(passport.context_minor), 0);
            const char* title = passport.title.empty() ? "Raidenmamare" : passport.title.c_str();

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, context_major);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, context_minor);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
            if (!window) {
                glfwTerminate();
                throw std::runtime_error("Device::Materializer::materialize: glfwCreateWindow() failed");
            }

            // Rough center on a 5000×1400 display when using a ~3000×1200 client area (tweak if your WM differs).
            glfwSetWindowPos(window, 1000, 100);

            glfwMakeContextCurrent(window);
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                glfwDestroyWindow(window);
                glfwTerminate();
                throw std::runtime_error("Device::Materializer::materialize: glewInit() failed");
            }

            glEnable(GL_DEPTH_TEST);

            glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int framebuffer_width, int framebuffer_height) {
                glViewport(0, 0, framebuffer_width, framebuffer_height);
            });

            return window;
        }

        static void release_window(GLFWwindow* window) {
            if (!window) return;
            glfwDestroyWindow(window);
            glfwTerminate();
        }
    };

    void Device::Materializer::materialize(resources::Manager manager, Reading world, Id id) const {
        if (manager->materialized<Device>(id)) {
            throw std::runtime_error("Device::Materializer::materialize: resource is already materialized");
        }

        const auto& passport = ops::particle::get<Device>(world, id).passport;
        manager->layer<Device>().materialize(id, Device_private::init_window(passport));
    }

    void Device::Materializer::release(resources::Manager manager, Reading, Id id) const {
        auto& layer = manager->layer<Device>();
        GLFWwindow* window = layer.value(id);
        Device_private::release_window(window);
        layer.release(id);
    }

    void Device::Operations::present(Reading, Id id, resources::Manager manager) {
        glfwSwapBuffers(manager->layer<Device>().provide(id));
    }

    void Device::Operations::poll_events(Reading, Id, resources::Manager) {
        glfwPollEvents();
    }

    void Device::Operations::materialize(Reading world, Id id, resources::Manager manager) {
        ops::resource::materialize<Device>(world, manager, id);
    }

    void Device::Operations::release(Reading world, Id id, resources::Manager manager) {
        ops::resource::release<Device>(world, manager, id);
    }

    auto Device::Operations::provide(Reading, Id id, resources::Manager manager) -> RuntimeAccess {
        return manager->layer<Device>().provide(id);
    }

    const Invariants Device::invariants{
        .structural = {},
        .logical = {},
    };
}
