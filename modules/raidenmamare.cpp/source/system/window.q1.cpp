#include <Raidenmamare/system/window.q1.h>

#include <GLFW/glfw3.h>

namespace rmmr::system {

    using namespace fqsm::api;

    void Window::Actions::present(Reading context, Id deviceId) {
        glfwSwapBuffers(with<Device>::get(context, deviceId).handle);
    }

}
