#include <rmmr/wrapper/ui.h>

#include <imgui.h>

#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/system/core.q1.h>

#include <chrono>
#include <ctime>

namespace rmmr::wrapper::ui {

    using namespace fqsm::api;
    using namespace rmmr;

    void State::drawMainMenuBar(Product& product) {
        if (not ImGui::BeginMainMenuBar())
            return;

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Stats", nullptr, &stats);
            product.contributeViewMenu();
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    void State::drawStatsWindow(Writing world) {
        if (not stats)
            return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        constexpr float pad = 10.f;
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + pad, viewport->WorkPos.y + viewport->WorkSize.y - pad), ImGuiCond_Always, ImVec2(0.f, 1.f));
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.28f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 2.f));
        if (ImGui::Begin("Stats", &stats, flags)) {
            ImGui::SetWindowFontScale(0.75f);

            if (const auto core = with<system::Core>::singleton(world)) {
                const int64 now = with<system::Clock>::get(world, *core).absolute;
                int64 frame_us = 0;
                if (last_absolute > 0 and now >= last_absolute) {
                    frame_us = now - last_absolute;
                }
                last_absolute = now;

                const auto total_sec = now / 1'000'000;
                const auto hours = total_sec / 3600;
                const auto minutes = (total_sec / 60) % 60;
                const auto seconds = total_sec % 60;
                const auto fps = frame_us > 0 ? 1'000'000.0 / static_cast<double>(frame_us) : 0.0;

                ImGui::Text("Time: %02lld:%02lld:%02lld",
                    static_cast<long long>(hours),
                    static_cast<long long>(minutes),
                    static_cast<long long>(seconds));
                ImGui::Text("FPS: %.1f", fps);
                ImGui::Text("Frame: %.3f ms", static_cast<double>(frame_us) / 1000.0);
            } else {
                ImGui::TextDisabled("No system clock.");
            }

            ImGui::Separator();
            ImGui::Text("Materials: %zu", with<resource::material::Asset>::count(world));
            ImGui::Text("Textures: %zu", with<resource::texture::Asset>::count(world));

            const auto wall = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm local_tm{};
            localtime_s(&local_tm, &wall);
            ImGui::Text("Eltanin (Sigma Dracomis)  ·  %02d.%02d.%04d", local_tm.tm_mday, local_tm.tm_mon + 1, local_tm.tm_year + 1900);
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void State::draw(Writing world, Product& product) {
        drawMainMenuBar(product);
        drawStatsWindow(world);
        product.drawUi(world);
    }

}
