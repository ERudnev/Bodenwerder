#include <Raidenmamare/system/viewport.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>

namespace rmmr::system {

    using namespace fqsm::api;
    using namespace api_for_internals;

    namespace {
        auto device_for_viewport(Reading context, Viewport::Id viewportId) -> Device::Id {
            for (const auto entry : context->aspect<Viewport_group>().items()) {
                if (entry.value.contains(viewportId)) {
                    return entry.id;
                }
            }
            throw std::runtime_error("Viewport: viewport is not attached to a window");
        }

        auto glfw_handle_for_viewport(Reading context, Viewport::Id viewportId) -> GLFWwindow* {
            return with<Device>::get(context, device_for_viewport(context, viewportId)).handle;
        }
    } // namespace

    void Viewport::Actions::activate(Reading context, Id viewportId) {
        const auto& quantum = with<Viewport>::get(context, viewportId);
        glfwMakeContextCurrent(glfw_handle_for_viewport(context, viewportId));

        const int x = static_cast<int>(quantum.origin.x);
        const int y = static_cast<int>(quantum.origin.y);
        const int width = std::max(static_cast<int>(quantum.size.x), 1);
        const int height = std::max(static_cast<int>(quantum.size.y), 1);
        glViewport(x, y, width, height);
    }

    void Viewport::Actions::clear(Reading context, Id viewportId) {
        const auto& quantum = with<Viewport>::get(context, viewportId);
        glfwMakeContextCurrent(glfw_handle_for_viewport(context, viewportId));
        glClearColor(
            quantum.clear_color.x,
            quantum.clear_color.y,
            quantum.clear_color.z,
            quantum.clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

}
