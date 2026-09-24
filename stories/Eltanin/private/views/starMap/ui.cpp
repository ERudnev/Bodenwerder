#include "game.h"

#include <imgui.h>

#include <rmmr/wrapper/ui.h>

namespace eltanin {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        template<typename Panel>
        void togglePanel(const char* label, base::maybe<Panel>& panel) {
            bool open = panel.has_value();
            rmmr::wrapper::ui::viewToggle(label, &open);
            if (open == panel.has_value())
                return;
            if (open)
                panel = Panel{};
            else
                panel.reset();
        }

    }

    void Game::contributeViewMenu(Writing world) {
        if (uiMode == UiMode::locality) {
            if (ImGui::Button("Back"))
                closeLocalityScenario(world);
            contributeLocalityMenu(world);
            return;
        }
        if (ImGui::Button("Planet"))
            openPlanetScenario(world);
        togglePanel("Blueprints", starMap.menu.blueprints);
    }

    auto Game::activeOverlay() const -> base::maybe<rmmr::resource::overlay::Asset::Id> {
        if (uiMode != UiMode::starMap)
            return {};
        if (not starMap.menu.blueprints.has_value() or not blueprints.assets.editorEffect)
            return {};
        if (blueprints.state.membranes.enabled or blueprints.state.paletteMode)
            return {};
        return blueprints.assets.editorEffect;
    }

    auto Game::overlaySelection() const -> std::span<const rmmr::renderer::Integer32> {
        if (uiMode != UiMode::starMap)
            return {};
        if (not starMap.menu.blueprints.has_value() or blueprints.state.membranes.enabled or blueprints.state.paletteMode)
            return {};
        return blueprints.state.selection.aliases;
    }

    void Game::drawUi(Writing world) {
        if (uiMode == UiMode::locality) {
            drawLocalityUi(world);
            engageInputs(world);
            return;
        }
        if (starMap.menu.blueprints.has_value()) {
            bool open = true;
            blueprints.draw(world, open, blueprintPack, mountPack);
            if (not open)
                starMap.menu.blueprints.reset();
        }
        if (starMap.view)
            blueprints.bindView(views, starMap.menu.blueprints.has_value(), *starMap.view);
        engageInputs(world);

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        constexpr float pad = 10.0f;
        ImGui::SetNextWindowPos(ImVec2{viewport->WorkPos.x + viewport->WorkSize.x - pad, viewport->WorkPos.y + viewport->WorkSize.y - pad}, ImGuiCond_Always, ImVec2{1.0f, 1.0f});
        ImGui::SetNextWindowBgAlpha(0.48f);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{8.0f, 2.0f});
        if (ImGui::Begin("##starmapReadout", nullptr, flags)) {
            ImGui::SetWindowFontScale(0.75f);
            const auto& visuals = starMap.visuals;
            ImGui::Text("Scale   %.1f LY", visuals.scaleLy);
            ImGui::Text("Player  %.2f, %.2f, %.2f LY", visuals.player.x, visuals.player.y, visuals.player.z);
            ImGui::Text("Focus   %.2f, %.2f, %.2f LY", visuals.focus.x, visuals.focus.y, visuals.focus.z);
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

}
