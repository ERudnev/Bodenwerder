#include "story.h"

#include <imgui.h>

#include <rmmr/scene/camera.q1.h>
#include <rmmr/wrapper/ui.h>
#include <tommy/world.q1.h>

namespace tommy {

    using namespace fqsm::api;
    using namespace rmmr;

    void SpriteTest::contributeViewMenu(Writing) {
        rmmr::wrapper::ui::viewToggle("Camera", &ui.camera);
        rmmr::wrapper::ui::viewToggle("HUD", &ui.hud);
    }

    void SpriteTest::drawUi(Writing world) {
        drawHud(world);
        drawCameraWindow(world);
    }

    void SpriteTest::drawHud(Writing world) {
        if (not ui.hud) {
            return;
        }

        bool open = ui.hud;
        ImGui::SetNextWindowPos(ImVec2{16.0f, 40.0f}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("TomSawyer", &open)) {
            bool any = false;
            for (const auto [id, _] : world->aspect<World>().items()) {
                any = true;
                const auto& quantum = with<World>::get(world, id);
                ImGui::Text("World step: %d%s", quantum.step, quantum.paused ? "  [PAUSED]" : "");
                ImGui::Separator();
                ImGui::TextUnformatted("P — pause");
                break;
            }
            if (not any) {
                ImGui::TextDisabled("No world.");
            }
        }
        ImGui::End();
        ui.hud = open;
    }

    void SpriteTest::drawCameraWindow(Writing world) {
        if (not ui.camera or views.empty()) {
            return;
        }

        bool open = ui.camera;
        if (ImGui::Begin("Camera", &open)) {
            const auto camera = views.front().camera;
            if (not with<scene::Camera>::exists(world, camera)) {
                ImGui::TextDisabled("No camera selected.");
            } else {
                auto quantum = with<scene::Camera>::modify(world, camera);
                if (quantum->mode == scene::Camera::Mode::orthographic) {
                    int size[2] = {quantum->ortho_size.x, quantum->ortho_size.y};
                    if (ImGui::DragInt2("Ortho size", size, 1.0f, 1, 8192)) {
                        quantum->ortho_size = index2{size[0], size[1]};
                    }
                } else if (quantum->mode == scene::Camera::Mode::perspective) {
                    ImGui::SliderAngle("FoV H", &quantum->fov_x, 10.0f, 160.0f);
                } else {
                    ImGui::TextDisabled("Parallel projection (reserved).");
                }
                ImGui::DragFloat("Near", &quantum->z_near, 0.01f, 0.001f, quantum->z_far - 0.001f, "%.3f");
                ImGui::DragFloat("Far", &quantum->z_far, 0.1f, quantum->z_near + 0.001f, 10000.0f, "%.3f");
            }
        }
        ImGui::End();
        ui.camera = open;
    }

}
