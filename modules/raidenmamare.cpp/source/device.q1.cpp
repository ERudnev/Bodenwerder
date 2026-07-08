#include <Raidenmamare/device.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rmmr {
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
                throw std::runtime_error("Device::openWindow: glfwCreateWindow() failed");
            }

            glfwSetWindowPos(window, 1000, 100);
            glfwMakeContextCurrent(window);
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                glfwDestroyWindow(window);
                throw std::runtime_error("Device::openWindow: glewInit() failed");
            }

            glEnable(GL_DEPTH_TEST);

            glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int framebuffer_width, int framebuffer_height) {
                glViewport(0, 0, framebuffer_width, framebuffer_height);
            });

            return window;
        }

    } // namespace

    void Application::Actions::poll_events(Reading) {
        glfwPollEvents();
    }

    void Window::Actions::present(Reading context, Id id) {
        const Window::Handle handle = get(context, id).handle;
        if (!handle) {
            throw std::runtime_error("Window::Actions::present: handle is null");
        }
        glfwSwapBuffers(handle);
    }

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

    void Device::start(Writing) {
        if (!glfwInit()) {
            throw std::runtime_error("Device::start: glfwInit() failed");
        }
    }

    auto Device::openWindow(
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

    void Device::shutdown(Writing context) {
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
