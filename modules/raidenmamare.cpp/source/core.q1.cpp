#include <Raidenmamare/core.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>

namespace rmmr {
    struct Core_private : Core::Operations {
        using Passport = Core::Materializer::Passport;

        static auto init_window(const Passport& passport) -> GLFWwindow* {
            if (!glfwInit()) {
                throw std::runtime_error("Core::Materializer::materialize: glfwInit() failed");
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
                throw std::runtime_error("Core::Materializer::materialize: glfwCreateWindow() failed");
            }

            glfwMakeContextCurrent(window);
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                glfwDestroyWindow(window);
                glfwTerminate();
                throw std::runtime_error("Core::Materializer::materialize: glewInit() failed");
            }

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

    void Core::Materializer::materialize(resources::Manager manager, Reading world, Id id) const {
        if (manager->materialized<Core>(id)) {
            throw std::runtime_error("Core::Materializer::materialize: resource is already materialized");
        }

        const auto& passport = ops::particle::get<Core>(world, id).passport;
        manager->layer<Core>().materialize(id, Core_private::init_window(passport));
    }

    void Core::Materializer::release(resources::Manager manager, Reading, Id id) const {
        auto& layer = manager->layer<Core>();
        GLFWwindow* window = layer.value(id);
        Core_private::release_window(window);
        layer.release(id);
    }

    void Core::Operations::present(Reading, Id id, resources::Manager manager) {
        glfwSwapBuffers(manager->layer<Core>().provide(id));
    }

    void Core::Operations::poll_events(Reading, Id, resources::Manager) {
        glfwPollEvents();
    }

    void Core::Operations::materialize(Reading world, Id id, resources::Manager manager) {
        ops::resource::materialize<Core>(world, manager, id);
    }

    void Core::Operations::release(Reading world, Id id, resources::Manager manager) {
        ops::resource::release<Core>(world, manager, id);
    }

    auto Core::Operations::provide(Reading, Id id, resources::Manager manager) -> RuntimeAccess {
        return manager->layer<Core>().provide(id);
    }

    const Invariants Core::invariants{
        .structural = {},
        .logical = {},
    };
}
