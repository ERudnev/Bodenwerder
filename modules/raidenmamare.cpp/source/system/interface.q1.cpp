#include <GLFW/glfw3.h>

#include <vector>

#include <rmmr/system/imgui.q1.h>
#include <rmmr/system/interface.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/window.q1.h>

#include <base/logging.h>

namespace rmmr::system {

    using namespace fqsm::api;

    auto Interface::create(Writing context, string path, Core::GLVer version) -> Core::Id {
        return with<Core>::create(context, Core::Quantum{
            .assets_root = std::move(path),
            .version = version,
        });
    }

    auto Interface::createWindow(Writing context, decltype(Window::Quantum::title) title, index2 requested_size) -> Window::Id {
        return Window::Actions::create(context, std::move(title), requested_size);
    }

    void Interface::shutdown(Writing context) {
        std::vector<Viewport::Id> viewport_ids;
        for (const auto entry : context->aspect<Viewport>().items()) {
            viewport_ids.push_back(entry.id);
        }
        for (Viewport::Id viewport_id : viewport_ids) {
            with<Viewport>::remove(context, viewport_id);
        }

        std::vector<ImGuiHost::Id> imgui_host_ids;
        for (const auto entry : context->aspect<ImGuiHost>().items()) {
            imgui_host_ids.push_back(entry.id);
        }
        for (ImGuiHost::Id imgui_host_id : imgui_host_ids) {
            with<ImGuiHost>::remove(context, imgui_host_id);
        }

        std::vector<Device::Id> device_ids;
        for (const auto entry : context->aspect<Device>().items()) {
            device_ids.push_back(entry.id);
        }
        for (Device::Id device_id : device_ids) {
            with<Device>::remove(context, device_id);
        }
    }

}
