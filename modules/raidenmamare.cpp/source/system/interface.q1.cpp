#include <Raidenmamare/system/interface.q1.h>
#include <Raidenmamare/system/viewport.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rmmr::system {

    using namespace fqsm::api;

    namespace {

        auto ensure_core(Writing context) -> Core::Id {
            for (const auto entry : context.aspect<Core>().items()) {
                return entry.id;
            }
            return with<Core>::create(context, {});
        }

        auto create_glfw_handle(
            const Device::Global& device_global,
            const decltype(Window::Quantum::title)& title,
            const decltype(Window::Quantum::size)& size) -> GLFWwindow* {
            const int width = std::max(static_cast<int>(size.x), 1);
            const int height = std::max(static_cast<int>(size.y), 1);
            const int context_major = std::max(static_cast<int>(device_global.context_major), 1);
            const int context_minor = std::max(static_cast<int>(device_global.context_minor), 0);
            const char* window_title = title.empty() ? "Raidenmamare" : title.c_str();

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, context_major);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, context_minor);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            GLFWwindow* window = glfwCreateWindow(width, height, window_title, nullptr, nullptr);
            if (not window) {
                throw std::runtime_error("system::Interface::createWindow: glfwCreateWindow() failed");
            }

            glfwSetWindowPos(window, 1000, 100);
            glfwMakeContextCurrent(window);
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                glfwDestroyWindow(window);
                throw std::runtime_error("system::Interface::createWindow: glewInit() failed");
            }

            glEnable(GL_DEPTH_TEST);

            glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int framebuffer_width, int framebuffer_height) {
                glViewport(0, 0, framebuffer_width, framebuffer_height);
            });

            return window;
        }

    } // namespace

    auto Interface::Actions::createWindow(Writing context, decltype(Window::Quantum::title) title, decltype(Window::Quantum::size) size) -> Window::Id {
        if (not glfwInit()) {
            throw std::runtime_error("system::Interface::createWindow: glfwInit() failed");
        }

        const auto& device_global = with<Device>::get_global(context);
        const auto core = ensure_core(context);
        const auto handle = create_glfw_handle(device_global, title, size);

        const auto device = with<Device_group>::addElement(context, core, Device::Quantum{
            .handle = handle,
        });

        with<Window>::extend(context, device, Window::Quantum{
            .title = std::move(title),
            .size = size,
        });

        return device;
    }

    void Interface::Actions::shutdown(Writing context) {
        std::vector<Viewport::Id> viewport_ids;
        for (const auto entry : context.aspect<Viewport>().items()) {
            viewport_ids.push_back(entry.id);
        }
        for (Viewport::Id viewport_id : viewport_ids) {
            with<Viewport>::remove(context, viewport_id);
        }

        std::vector<Device::Id> device_ids;
        for (const auto entry : context.aspect<Device>().items()) {
            device_ids.push_back(entry.id);
        }
        for (Device::Id device_id : device_ids) {
            with<Device>::remove(context, device_id);
        }

        glfwTerminate();
    }

}
