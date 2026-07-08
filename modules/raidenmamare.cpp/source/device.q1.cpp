#include <Raidenmamare/device.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rmmr {
    struct Window::Internals : Window::DefaultInternals {
        static void release(Writing, Id, const Quantum& last) {
            if (!last.handle) {
                return;
            }
            glfwDestroyWindow(last.handle);
        }
    };

    auto Window::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Window>(&Internals::release),
        };
    }

    struct Device::Internals : Device::DefaultInternals {
        static bool has_window(Reading context, Window::Id window) {
            return with<Window>::exists(context, window);
        }

        static auto resolved_handle(Reading context, Window::Id window) -> Window::Handle {
            return with<Window>::get(context, window).handle;
        }

        static auto create_handle(
            const Quantum& device,
            const decltype(Window::Quantum::title)& title,
            const decltype(Window::Quantum::size)& size) -> Window::Handle {
            if (!glfwInit()) {
                throw std::runtime_error("Device::Actions::init: glfwInit() failed");
            }

            const int width = std::max(static_cast<int>(size.x), 1);
            const int height = std::max(static_cast<int>(size.y), 1);
            const int context_major = std::max(static_cast<int>(device.context_major), 1);
            const int context_minor = std::max(static_cast<int>(device.context_minor), 0);
            const char* window_title = title.empty() ? "Raidenmamare" : title.c_str();

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, context_major);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, context_minor);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            GLFWwindow* window = glfwCreateWindow(width, height, window_title, nullptr, nullptr);
            if (!window) {
                glfwTerminate();
                throw std::runtime_error("Device::Actions::init: glfwCreateWindow() failed");
            }

            glfwSetWindowPos(window, 1000, 100);
            glfwMakeContextCurrent(window);
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                glfwDestroyWindow(window);
                glfwTerminate();
                throw std::runtime_error("Device::Actions::init: glewInit() failed");
            }

            glEnable(GL_DEPTH_TEST);

            glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int framebuffer_width, int framebuffer_height) {
                glViewport(0, 0, framebuffer_width, framebuffer_height);
            });

            return window;
        }

        static void deinit(Writing context, Id, const Quantum& last) {
            if (has_window(context, last.window)) {
                with<Window>::remove(context, last.window);
            }
            glfwTerminate();
        }
    };

    void Device::Actions::present(Reading context, Id id) {
        const auto& quantum = get(context, id);
        if (!Internals::has_window(context, quantum.window)) {
            throw std::runtime_error("Device::Actions::present: window is not initialized");
        }
        glfwSwapBuffers(Internals::resolved_handle(context, quantum.window));
    }

    void Device::Actions::poll_events(Reading, Id) {
        glfwPollEvents();
    }

    void Device::Actions::init(
        Writing context,
        Id id,
        decltype(Window::Quantum::title) title,
        decltype(Window::Quantum::size) size) {
        const auto& before = get(context, id);
        if (Internals::has_window(context, before.window)) {
            return;
        }

        const auto handle = Internals::create_handle(before, title, size);
        const auto windowId = with<Window>::create(context, Window::Quantum{
            .title = std::move(title),
            .size = size,
            .handle = handle,
        });
        modify(context, id)->window = windowId;
    }

    auto Device::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::controls<Device, Window, &Device::Quantum::window>{},
            reaction::deletion<Device>(&Internals::deinit),
        };
    }
}
