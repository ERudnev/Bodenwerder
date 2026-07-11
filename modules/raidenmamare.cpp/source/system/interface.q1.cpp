#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <vector>

#include <Raidenmamare/system/interface.q1.h>
#include <Raidenmamare/system/viewport.q1.h>
#include <Raidenmamare/system/window.q1.h>

namespace rmmr::system {

    using namespace fqsm::api;

    auto Interface::create(Writing context, string path, Core::GLVer version) -> Core::Id {
        return with<Core>::create(context, Core::Quantum{
            .assets_root = std::move(path),
            .version = version,
        });
    }

    auto Interface::createWindow(Writing context, decltype(Window::Quantum::title) title, decltype(Window::Quantum::size) size) -> Window::Id {
        return Window::Actions::create(context, std::move(title), size);
    }

    void Interface::shutdown(Writing context) {
        std::vector<Viewport::Id> viewport_ids;
        for (const auto entry : context->aspect<Viewport>().items()) {
            viewport_ids.push_back(entry.id);
        }
        for (Viewport::Id viewport_id : viewport_ids) {
            with<Viewport>::remove(context, viewport_id);
        }

        std::vector<Device::Id> device_ids;
        for (const auto entry : context->aspect<Device>().items()) {
            device_ids.push_back(entry.id);
        }
        for (Device::Id device_id : device_ids) {
            with<Device>::remove(context, device_id);
        }

        glfwTerminate();
    }

}
