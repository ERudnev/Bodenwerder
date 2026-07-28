#include "story.h"

#include <imgui.h>

#include <rmmr/scene/camera.q1.h>
#include <tommy/invaders/session.h>
#include <tommy/world.h>

namespace tommy {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto phaseName(invaders::Phase phase) -> const char* {
            switch (phase) {
                case invaders::Phase::attract: return "ATTRACT";
                case invaders::Phase::playing: return "PLAYING";
                case invaders::Phase::wave_clear: return "WAVE CLEAR";
                case invaders::Phase::lost: return "GAME OVER";
                case invaders::Phase::won: return "YOU WIN";
            }
            return "?";
        }

    } // namespace

    void SpriteTest::contributeViewMenu() {
        ImGui::MenuItem("Camera", nullptr, &ui.camera);
        ImGui::MenuItem("HUD", nullptr, &ui.hud);
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
        if (ImGui::Begin("Space Invaders", &open)) {
            bool paused = false;
            for (const auto [id, _] : world->aspect<World>().items()) {
                paused = with<World>::get(world, id).paused;
                ImGui::Text("World step: %d%s", with<World>::get(world, id).step, paused ? "  [PAUSED]" : "");
                break;
            }

            bool any = false;
            for (const auto [id, _] : world->aspect<invaders::Session>().items()) {
                any = true;
                const auto& session = with<invaders::Session>::get(world, id);
                ImGui::Separator();
                ImGui::Text("Phase: %s", phaseName(session.phase));
                ImGui::Text("Score: %d", session.score);
                ImGui::Text("Lives: %d", session.lives);
                ImGui::Text("Wave:  %d", session.wave);
                ImGui::Separator();
                ImGui::TextUnformatted("A/D or arrows — move (in play)");
                ImGui::TextUnformatted("Arrows / RMB — pan camera (menu)");
                ImGui::TextUnformatted("Space / W — fire");
                ImGui::TextUnformatted("P — pause");
                ImGui::TextUnformatted("Enter — start (attract)");
                ImGui::TextUnformatted("R — restart after game over");
            }
            if (not any) {
                ImGui::TextDisabled("No session.");
            }
        }
        ImGui::End();
        ui.hud = open;
    }

    void SpriteTest::drawCameraWindow(Writing world) {
        if (not ui.camera or views.empty())
            return;

        bool open = ui.camera;
        if (ImGui::Begin("Camera", &open)) {
            const auto camera = views.front().camera;
            if (not with<scene::Camera>::exists(world, camera)) {
                ImGui::TextDisabled("No camera selected.");
            } else {
                auto quantum = with<scene::Camera>::modify(world, camera);
                if (quantum->mode == scene::Camera::Mode::orthographic) {
                    int size[2] = {quantum->ortho_size.x, quantum->ortho_size.y};
                    if (ImGui::DragInt2("Ortho size", size, 1.0f, 1, 8192))
                        quantum->ortho_size = index2{size[0], size[1]};
                } else if (quantum->mode == scene::Camera::Mode::perspective) {
                    ImGui::SliderAngle("FoV", &quantum->fov_y, 10.0f, 160.0f);
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
