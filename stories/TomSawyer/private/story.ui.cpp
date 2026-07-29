#include "story.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

#include <rmmr/scene/camera.q1.h>
#include <tommy/gameObject.h>
#include <tommy/gun.h>
#include <tommy/player.h>
#include <tommy/world.h>

namespace tommy {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto clamp01(float value) -> float {
            return std::clamp(value, 0.0f, 1.0f);
        }

    } // namespace

    void SpriteTest::contributeViewMenu() {
        ImGui::MenuItem("Camera", nullptr, &ui.camera);
        ImGui::MenuItem("HUD", nullptr, &ui.hud);
        ImGui::MenuItem("Ship", nullptr, &ui.ship);
    }

    void SpriteTest::drawUi(Writing world) {
        drawHud(world);
        drawShipPanel(world);
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
                ImGui::TextUnformatted("WASD / arrows — thrust + turn");
                ImGui::TextUnformatted("Space — fire");
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

    void SpriteTest::drawShipPanel(Writing world) {
        if (not ui.ship) {
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

        bool open = ui.ship;
        if (not ImGui::Begin("SHIP", &open, flags)) {
            ImGui::End();
            ui.ship = open;
            return;
        }

        bool drawn = false;
        for (const auto entry : world->aspect<Player>().items()) {
            const auto id = entry.id;
            if (not with<Physical>::exists(world, id)) {
                continue;
            }
            drawn = true;
            const auto& physical = with<Physical>::get(world, id);
            const auto& player = with<Player>::get(world, id);
            const float hull = clamp01(
                static_cast<float>(physical.hitpoints)
                / static_cast<float>(Player::max_hitpoints));
            const bool critical = physical.hitpoints <= Player::max_hitpoints / 4;
            const bool dead = physical.hitpoints <= 0;

            ImGui::SetWindowFontScale(1.35f);
            if (dead) {
                ImGui::TextColored(ImVec4{1.0f, 0.25f, 0.20f, 1.0f}, "HULL  DESTROYED");
            } else if (critical) {
                ImGui::TextColored(ImVec4{1.0f, 0.55f, 0.20f, 1.0f}, "HULL  CRITICAL");
            } else {
                ImGui::Text("HULL  %d / %d", physical.hitpoints, Player::max_hitpoints);
            }
            ImGui::SetWindowFontScale(1.0f);

            char hull_overlay[64];
            std::snprintf(
                hull_overlay,
                sizeof(hull_overlay),
                "%d / %d",
                physical.hitpoints,
                Player::max_hitpoints);

            ImVec4 hull_color{0.25f, 0.85f, 0.40f, 1.0f};
            if (dead) {
                hull_color = ImVec4{0.45f, 0.12f, 0.10f, 1.0f};
            } else if (critical) {
                hull_color = ImVec4{1.0f, 0.35f, 0.20f, 1.0f};
            } else if (hull < 0.55f) {
                hull_color = ImVec4{1.0f, 0.75f, 0.20f, 1.0f};
            }

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hull_color);
            ImGui::ProgressBar(hull, ImVec2{-1.0f, 44.0f}, hull_overlay);
            ImGui::PopStyleColor();

            if (not with<Gun>::exists(world, player.gun)) {
                ImGui::Spacing();
                ImGui::TextDisabled("No gun.");
                break;
            }

            const auto& gun = with<Gun>::get(world, player.gun);
            const integer now = with<World>::exists(world, gun.world)
                ? with<World>::get(world, gun.world).step
                : 0;

            const integer mech_left = std::max(integer{0}, gun.mech_ready_at - now);
            const float mech_ready = clamp01(
                1.0f - static_cast<float>(mech_left) / static_cast<float>(Gun::mech_cooldown_steps));
            const float heat = clamp01(
                static_cast<float>(gun.temperature_celsius)
                / static_cast<float>(Gun::temp_max_celsius));
            const float heat_gate = static_cast<float>(Gun::fire_below_celsius)
                / static_cast<float>(Gun::temp_max_celsius);
            const bool overheated = gun.temperature_celsius >= Gun::fire_below_celsius;

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

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
                ImGui::Text("HEAT  %d / %d C", gun.temperature_celsius, Gun::temp_max_celsius);
            }
            ImGui::SetWindowFontScale(1.0f);

            char heat_overlay[64];
            std::snprintf(
                heat_overlay,
                sizeof(heat_overlay),
                "%d C  (fire < %d)",
                gun.temperature_celsius,
                Gun::fire_below_celsius);

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
            ImGui::TextDisabled("No ship.");
        }

        ImGui::End();
        ui.ship = open;
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
