#include "story.h"

#include <algorithm>
#include <cstdio>

#include <imgui.h>

#include <rmmr/scene/camera.q1.h>
#include <si01/invaders/actors.h>
#include <si01/invaders/gun.h>
#include <si01/invaders/session.h>
#include <si01/world.h>

namespace si01 {

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

        auto clamp01(float value) -> float {
            return std::clamp(value, 0.0f, 1.0f);
        }

    } // namespace

    void SpriteTest::contributeViewMenu() {
        ImGui::MenuItem("Camera", nullptr, &ui.camera);
        ImGui::MenuItem("HUD", nullptr, &ui.hud);
        ImGui::MenuItem("Gun", nullptr, &ui.gun);
    }

    void SpriteTest::drawUi(Writing world) {
        drawHud(world);
        drawGunPanel(world);
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

    void SpriteTest::drawGunPanel(Writing world) {
        if (not ui.gun) {
            return;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2{viewport->WorkPos.x + viewport->WorkSize.x - 16.0f, viewport->WorkPos.y + 48.0f},
            ImGuiCond_Always,
            ImVec2{1.0f, 0.0f});
        ImGui::SetNextWindowSize(ImVec2{320.0f, 0.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.82f);

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav;

        bool open = ui.gun;
        if (not ImGui::Begin("GUN", &open, flags)) {
            ImGui::End();
            ui.gun = open;
            return;
        }

        bool drawn = false;
        for (const auto [session_id, _] : world->aspect<invaders::Session>().items()) {
            const auto& session = with<invaders::Session>::get(world, session_id);
            if (not session.player or not with<invaders::Player>::exists(world, *session.player)) {
                continue;
            }
            const auto& player = with<invaders::Player>::get(world, *session.player);
            if (not with<invaders::Gun>::exists(world, player.gun)) {
                continue;
            }
            drawn = true;
            const auto& gun = with<invaders::Gun>::get(world, player.gun);
            const integer now = with<World>::exists(world, gun.world)
                ? with<World>::get(world, gun.world).step
                : 0;

            const integer mech_left = std::max(integer{0}, gun.mech_ready_at - now);
            const float mech_ready = clamp01(
                1.0f - static_cast<float>(mech_left) / static_cast<float>(invaders::Gun::mech_cooldown_steps));
            const float heat = clamp01(
                static_cast<float>(gun.temperature_celsius)
                / static_cast<float>(invaders::Gun::temp_max_celsius));
            const float heat_gate = static_cast<float>(invaders::Gun::fire_below_celsius)
                / static_cast<float>(invaders::Gun::temp_max_celsius);
            const bool overheated = gun.temperature_celsius >= invaders::Gun::fire_below_celsius;

            ImGui::SetWindowFontScale(1.35f);
            ImGui::TextUnformatted("RATE");
            ImGui::SetWindowFontScale(1.0f);

            char mech_overlay[64];
            if (mech_left <= 0) {
                std::snprintf(mech_overlay, sizeof(mech_overlay), "READY");
            } else {
                std::snprintf(
                    mech_overlay,
                    sizeof(mech_overlay),
                    "%.2f s",
                    static_cast<float>(mech_left) / 1000.0f);
            }
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, mech_ready >= 1.0f
                ? ImVec4{0.25f, 0.85f, 0.40f, 1.0f}
                : ImVec4{0.35f, 0.65f, 1.0f, 1.0f});
            ImGui::ProgressBar(mech_ready, ImVec2{-1.0f, 36.0f}, mech_overlay);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::SetWindowFontScale(1.35f);
            if (overheated) {
                ImGui::TextColored(ImVec4{1.0f, 0.35f, 0.25f, 1.0f}, "HEAT  OVERHEAT");
            } else {
                ImGui::Text("HEAT  %d / %d C", gun.temperature_celsius, invaders::Gun::temp_max_celsius);
            }
            ImGui::SetWindowFontScale(1.0f);

            char heat_overlay[64];
            std::snprintf(
                heat_overlay,
                sizeof(heat_overlay),
                "%d C  (fire < %d)",
                gun.temperature_celsius,
                invaders::Gun::fire_below_celsius);

            ImVec4 heat_color{0.30f, 0.90f, 0.45f, 1.0f};
            if (heat >= heat_gate) {
                heat_color = ImVec4{1.0f, 0.25f, 0.20f, 1.0f};
            } else if (heat >= heat_gate * 0.65f) {
                heat_color = ImVec4{1.0f, 0.75f, 0.20f, 1.0f};
            }

            const ImVec2 bar_min = ImGui::GetCursorScreenPos();
            const float bar_w = ImGui::GetContentRegionAvail().x;
            constexpr float bar_h = 44.0f;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, heat_color);
            ImGui::ProgressBar(heat, ImVec2{-1.0f, bar_h}, heat_overlay);
            ImGui::PopStyleColor();

            // Gate mark on the heat bar (fire_below / temp_max).
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const float gate_x = bar_min.x + bar_w * heat_gate;
            draw->AddLine(
                ImVec2{gate_x, bar_min.y},
                ImVec2{gate_x, bar_min.y + bar_h},
                IM_COL32(255, 255, 255, 220),
                2.0f);

            break;
        }

        if (not drawn) {
            ImGui::TextDisabled("No gun.");
        }

        ImGui::End();
        ui.gun = open;
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
