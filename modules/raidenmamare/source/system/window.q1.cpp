#include <rmmr/system/window.q1.h>

#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/system/imgui.q1.h>
#include <rmmr/system/viewport.q1.h>

#include <base/logging.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rmmr::system {
    using namespace fqsm::api;
    using namespace api_for_internals;
    namespace {

        constexpr integer k_glfw_key_capacity = GLFW_KEY_LAST + 1;
        constexpr integer k_glfw_button_capacity = GLFW_MOUSE_BUTTON_LAST + 1;

        void APIENTRY debugMessage(GLenum, GLenum, GLuint, GLenum severity, GLsizei, const GLchar* message, const void*) {
            if (severity == GL_DEBUG_SEVERITY_HIGH or severity == GL_DEBUG_SEVERITY_MEDIUM) {
                base::message("OpenGL: {}", message);
            }
        }

        auto empty_input_state() -> Window::InputState {
            return Window::InputState{
                .keys = {},
                .buttons = {},
                .mouse = index2{0, 0},
                .wheel = 0.0f,
                .under = renderer::Integer32{0},
            };
        }

        void bootstrap_device(Writing context, Device::Id device) {
            with<Viewport_group>::extend(context, device);
            with<resource::Runtimes>::install(context, device);
        }

        auto create_glfw_handle(const Core::GLVer& version, const string& title, const index2& requested_size, Window::Presentation presentation) -> GLFWwindow* {
            if (version.major != 4 or version.minor != 6) {
                throw std::runtime_error("system::Window::create: Raidenmamare requires OpenGL 4.6 core");
            }
            const int width = std::max(static_cast<int>(requested_size.x), 1);
            const int height = std::max(static_cast<int>(requested_size.y), 1);
            const int context_major = std::max(static_cast<int>(version.major), 1);
            const int context_minor = std::max(static_cast<int>(version.minor), 0);
            const char* window_title = title.empty() ? "Raidenmamare" : title.c_str();

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, context_major);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, context_minor);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            GLFWwindow* window = glfwCreateWindow(width, height, window_title, nullptr, nullptr);
            if (not window) {
                throw std::runtime_error("system::Window::create: glfwCreateWindow() failed");
            }

            if (presentation == Window::Presentation::maximized)
                glfwMaximizeWindow(window);
            else
                glfwSetWindowPos(window, 1000, 100);

            glfwMakeContextCurrent(window);
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                glfwDestroyWindow(window);
                throw std::runtime_error("system::Window::create: glewInit() failed");
            }
            GLint actualMajor = 0;
            GLint actualMinor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &actualMajor);
            glGetIntegerv(GL_MINOR_VERSION, &actualMinor);
            if (actualMajor < 4 or (actualMajor == 4 and actualMinor < 6)) {
                glfwDestroyWindow(window);
                throw std::runtime_error("system::Window::create: OpenGL 4.6 core context unavailable");
            }

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEBUG_OUTPUT);
            glDebugMessageCallback(debugMessage, nullptr);

            return window;
        }

        void poll_input(GLFWwindow* handle, Window::InputState& input) {
            if (input.keys.size() < static_cast<std::size_t>(k_glfw_key_capacity)) {
                input.keys.assign(static_cast<std::size_t>(k_glfw_key_capacity), false);
            }
            if (input.buttons.size() < static_cast<std::size_t>(k_glfw_button_capacity)) {
                input.buttons.assign(static_cast<std::size_t>(k_glfw_button_capacity), false);
            }

            for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key) {
                input.keys[static_cast<std::size_t>(key)] = glfwGetKey(handle, key) == GLFW_PRESS;
            }
            for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; ++button) {
                input.buttons[static_cast<std::size_t>(button)] = glfwGetMouseButton(handle, button) == GLFW_PRESS;
            }

            double mouse_x = 0.0;
            double mouse_y = 0.0;
            glfwGetCursorPos(handle, &mouse_x, &mouse_y);
            input.mouse = index2{static_cast<integer>(std::lround(mouse_x)), static_cast<integer>(std::lround(mouse_y))};
            input.wheel = 0.0f; // filled after ImGui::NewFrame in applyUiCapture
        }

    } // namespace

    auto Window::Actions::create(Writing context, string title, index2 requested_size, Presentation presentation) -> Id {
        if (not glfwInit()) {
            throw std::runtime_error("system::Window::create: glfwInit() failed");
        }

        const auto core = with<Core>::singleton(context);
        if (not core) throw std::runtime_error("system::Window::create: Core singleton missing");
        const auto& core_quantum = with<Core>::get(context, *core);
        const auto handle = create_glfw_handle(core_quantum.version, title, requested_size, presentation);

        const auto device = with<Device>::create(context, Device::Quantum{
            .core = *core,
            .handle = handle,
        });

        with<Window>::extend(context, device, Window::Quantum{
            .title = std::move(title),
            .previous = empty_input_state(),
            .current = empty_input_state(),
            .identityDraws = 0,
        });
        with<ImGuiHost>::extend(context, device, ImGuiHost::Quantum{
            .context = nullptr,
        });

        bootstrap_device(context, device);
        with<ImGuiHost>::initialize(context, device);

        return device;
    }

    auto Window::Actions::framebufferSize(Reading context, Id window) -> index2 {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(with<Device>::get(context, window).handle, &width, &height);
        return index2{std::max(width, 1), std::max(height, 1)};
    }

    void Window::Actions::present(Reading context, Id window) {
        glfwSwapBuffers(with<Device>::get(context, window).handle);
    }

    auto Window::Actions::mouseShift(Reading context, Id window) -> index2 {
        const auto& quantum = with<Window>::get(context, window);
        if (quantum.previous.keys.empty()) {
            return index2{0, 0};
        }
        return index2{quantum.current.mouse.x - quantum.previous.mouse.x, quantum.current.mouse.y - quantum.previous.mouse.y};
    }

    void Window::Actions::onFrameAdvanced(Writing context, Id window) {
        auto quantum = with<Window>::modify(context, window);
        quantum->previous = quantum->current;
        poll_input(with<Device>::get(context, window).handle, quantum->current);
    }

    void Window::Actions::applyUiCapture(Writing context, Id window) {
        const auto& io = ImGui::GetIO();
        auto quantum = with<Window>::modify(context, window);
        quantum->current.wheel = io.WantCaptureMouse ? 0.0f : io.MouseWheel;
        if (io.WantCaptureKeyboard) {
            std::fill(quantum->current.keys.begin(), quantum->current.keys.end(), false);
        }
        if (io.WantCaptureMouse) {
            // Neutral delta for look/drag; drop buttons so RMB does not stick through UI.
            quantum->current.mouse = quantum->previous.mouse;
            std::fill(quantum->current.buttons.begin(), quantum->current.buttons.end(), false);
        }
    }

}
