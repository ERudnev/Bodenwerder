#include <rmmr/system/imgui.q1.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <base/logging.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace rmmr::system {

    using namespace fqsm::api;

    namespace {

        auto glsl_version_string(const Core::GLVer& version) -> string {
            const int major = std::max(static_cast<int>(version.major), 1);
            const int minor = std::max(static_cast<int>(version.minor), 0);
            return string("#version ") + std::to_string(major * 100 + minor * 10);
        }

        void make_context_current(fqsm::Reading context, Device::Id device) {
            const auto handle = with<Device>::get(context, device).handle;
            if (not handle) {
                throw std::runtime_error("system::ImGuiHost: device has no GLFW window");
            }
            glfwMakeContextCurrent(handle);
        }

    } // namespace

    struct ImGuiHost::Internals : ImGuiHost::DefaultInternals {
        static void release(fqsm::Retrospecting context, Id id, const Quantum& last) {
            if (not last.context) {
                return;
            }

            make_context_current(context, id);
            ImGui::SetCurrentContext(last.context);
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext(last.context);
            base::message("rmmr teardown: ImGuiHost shutdown done id={}", id);
        }
    };

    void ImGuiHost::Actions::initialize(fqsm::Writing context, Id id) {
        auto host = with<ImGuiHost>::modify(context, id);
        if (host->context) {
            return;
        }

        make_context_current(context, id);
        const auto& device = with<Device>::get(context, id);
        const auto& core = with<Core>::get(context, device.core);
        const string glsl_version = glsl_version_string(core.version);

        IMGUI_CHECKVERSION();
        ImGuiContext* const imgui_context = ImGui::CreateContext();
        ImGui::SetCurrentContext(imgui_context);

        if (not ImGui_ImplGlfw_InitForOpenGL(device.handle, true)) {
            ImGui::DestroyContext(imgui_context);
            throw std::runtime_error("system::ImGuiHost::initialize: ImGui GLFW backend init failed");
        }
        if (not ImGui_ImplOpenGL3_Init(glsl_version.c_str())) {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext(imgui_context);
            throw std::runtime_error("system::ImGuiHost::initialize: ImGui OpenGL3 backend init failed");
        }

        host->context = imgui_context;
        base::message("rmmr: ImGuiHost::initialize id={} context={}", id, static_cast<void*>(imgui_context));
    }

    void ImGuiHost::Actions::newFrame(fqsm::Reading context, Id id) {
        const auto& host = with<ImGuiHost>::get(context, id);
        if (not host.context) {
            throw std::runtime_error("system::ImGuiHost::newFrame: host is not initialized");
        }

        make_context_current(context, id);
        ImGui::SetCurrentContext(host.context);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiHost::Actions::render(fqsm::Reading context, Id id) {
        const auto& host = with<ImGuiHost>::get(context, id);
        if (not host.context) {
            throw std::runtime_error("system::ImGuiHost::render: host is not initialized");
        }

        make_context_current(context, id);
        ImGui::SetCurrentContext(host.context);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    auto ImGuiHost::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<ImGuiHost>(&Internals::release),
        };
    }

}
