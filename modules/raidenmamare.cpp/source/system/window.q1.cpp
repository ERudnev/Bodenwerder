#include <Raidenmamare/system/window.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace rmmr::system {
    using namespace fqsm::api;
    using namespace api_for_internals;
    namespace {

        constexpr integer k_glfw_key_capacity = GLFW_KEY_LAST + 1;

        auto empty_input_state() -> Window::InputState {
            return Window::InputState{
                .frame = 0,
                .clock = Window::time{},
                .keys = {},
                .mouse = index2{0, 0},
            };
        }

        auto clock_from_glfw() -> Window::time {
            return Window::time{} + std::chrono::duration_cast<Window::time::duration>(std::chrono::duration<double>(glfwGetTime()));
        }

        auto ensure_core(Writing context) -> Core::Id {
            for (const auto entry : context->aspect<Core>().items()) {
                return entry.id;
            }
            return with<Core>::create(context, {});
        }

        auto create_glfw_handle(const Device::Global& device_global, const string& title, const index2& size) -> GLFWwindow* {
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
                throw std::runtime_error("system::Window::create: glfwCreateWindow() failed");
            }

            glfwSetWindowPos(window, 1000, 100);
            glfwMakeContextCurrent(window);
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                glfwDestroyWindow(window);
                throw std::runtime_error("system::Window::create: glewInit() failed");
            }

            glEnable(GL_DEPTH_TEST);

            glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int framebuffer_width, int framebuffer_height) {
                glViewport(0, 0, framebuffer_width, framebuffer_height);
            });

            return window;
        }

        void poll_input(GLFWwindow* handle, Window::InputState& input) {
            if (input.keys.size() < static_cast<std::size_t>(k_glfw_key_capacity)) {
                input.keys.assign(static_cast<std::size_t>(k_glfw_key_capacity), false);
            }

            for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key) {
                input.keys[static_cast<std::size_t>(key)] = glfwGetKey(handle, key) == GLFW_PRESS;
            }

            double mouse_x = 0.0;
            double mouse_y = 0.0;
            glfwGetCursorPos(handle, &mouse_x, &mouse_y);
            input.mouse = index2{static_cast<integer>(std::lround(mouse_x)), static_cast<integer>(std::lround(mouse_y))};
        }

    } // namespace

    auto Window::Actions::create(Writing context, string title, index2 size) -> Id {
        if (not glfwInit()) {
            throw std::runtime_error("system::Window::create: glfwInit() failed");
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
            .previous = empty_input_state(),
            .current = empty_input_state(),
        });

        return device;
    }

    void Window::Actions::present(Reading context, Id window) {
        glfwSwapBuffers(with<Device>::get(context, window).handle);
    }

    auto Window::Actions::dt(Reading context, Id window) -> seconds {
        const auto& quantum = with<Window>::get(context, window);
        if (quantum.previous.frame <= 0) {
            return seconds{0.0};
        }
        return std::chrono::duration<double>(quantum.current.clock - quantum.previous.clock).count();
    }

    auto Window::Actions::mouseShift(Reading context, Id window) -> index2 {
        const auto& quantum = with<Window>::get(context, window);
        if (quantum.previous.frame <= 0) {
            return index2{0, 0};
        }
        return index2{quantum.current.mouse.x - quantum.previous.mouse.x, quantum.current.mouse.y - quantum.previous.mouse.y};
    }

    void Window::Actions::onFrameAdvanced(Writing context, Id window) {
        auto quantum = with<Window>::modify(context, window);
        quantum->previous = quantum->current;

        auto& next = quantum->current;
        next.frame = quantum->previous.frame + 1;
        next.clock = clock_from_glfw();
        poll_input(with<Device>::get(context, window).handle, next);
    }

}
