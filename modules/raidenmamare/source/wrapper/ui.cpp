#include <rmmr/wrapper/ui.h>

#include <rmmr/system/core.q1.h>
#include <rmmr/system/window.q1.h>

#include <imgui.h>

#include <chrono>
#include <ctime>

namespace rmmr::wrapper::ui {

    using namespace fqsm::api;
    using namespace rmmr;

    void State::drawViewToolbar(Writing world, Product& product) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2{viewport->WorkPos.x + 8.f, viewport->WorkPos.y + 8.f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{6.f, 4.f});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{4.f, 4.f});
        if (ImGui::Begin("##viewToolbar", nullptr, flags))
            product.contributeViewMenu(world);
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void State::drawStatsWindow(Writing world) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        constexpr float pad = 10.f;
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + pad, viewport->WorkPos.y + viewport->WorkSize.y - pad), ImGuiCond_Always, ImVec2(0.f, 1.f));
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.48f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 2.f));
        if (ImGui::Begin("Stats", nullptr, flags)) {
            ImGui::SetWindowFontScale(0.75f);

            const auto core = with<system::Core>::singleton(world);
            const int64 now = with<system::Clock>::get(world, core).absolute;
            int64 frameUs = 0;
            if (lastAbsolute > 0 and now >= lastAbsolute)
                frameUs = now - lastAbsolute;
            lastAbsolute = now;

            const auto totalSec = now / 1'000'000;
            const auto hours = totalSec / 3600;
            const auto minutes = (totalSec / 60) % 60;
            const auto seconds = totalSec % 60;
            const auto fps = frameUs > 0 ? 1'000'000.0 / static_cast<double>(frameUs) : 0.0;

            ImGui::Text("Time: %02lld:%02lld:%02lld", static_cast<long long>(hours), static_cast<long long>(minutes), static_cast<long long>(seconds));
            ImGui::Text("FPS: %.1f", fps);
            ImGui::Text("Frame: %.3f ms", static_cast<double>(frameUs) / 1000.0);

            integer identityDraws = 0;
            for (const auto [_, window] : world->aspect<system::Window>().items()) {
                identityDraws = window.identityDraws;
                break;
            }
            ImGui::Text("Identity: %lld", static_cast<long long>(identityDraws));

            const auto wall = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm localTm{};
            localtime_s(&localTm, &wall);
            ImGui::Text("Eltanin (Gamma Dracomis)  ·  %02d.%02d.%04d", localTm.tm_mday, localTm.tm_mon + 1, localTm.tm_year + 1900);
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void State::draw(Writing world, Product& product) {
        drawViewToolbar(world, product);
        drawStatsWindow(world);
        product.drawUi(world);
    }

}
