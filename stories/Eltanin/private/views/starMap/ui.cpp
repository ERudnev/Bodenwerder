#include "game.h"

#include <eltanin/fundamental/existent.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/window.q1.h>
#include <rmmr/wrapper/ui.h>
#include <algorithm>

#include <GLFW/glfw3.h>
#include <imgui.h>

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

        void setWarp(Writing world, integer warp) {
            warp = std::clamp(warp, integer{0}, fundamental::Existent::Always::warpTop);
            with<fundamental::Existent>::modify_global(world)->warp = warp;
            with<World>::modify_global(world)->paused = warp == 0;
        }

    }

    void Game::contributeChrome(Writing world) {
        const bool menuOpen = ui.menu.has_value();
        if (menuOpen) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button("Menu")) {
            if (menuOpen)
                ui.menu.reset();
            else
                ui.menu.emplace();
        }
        if (menuOpen)
            ImGui::PopStyleColor(2);

        const auto& existent = with<fundamental::Existent>::get_global(world);
        const auto civil = fundamental::Existent::Always::civil(existent.now);
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{6.f, 0.f});
        ImGui::SetWindowFontScale(0.72f);
        ImGui::Text("Y %d  M %02d  D %02d", civil.year, civil.month, civil.day);
        ImGui::Text("%02d:%02d:%02d", civil.hour, civil.minute, civil.second);
        ImGui::SetWindowFontScale(1.f);
        ImGui::PopStyleVar();
        ImGui::EndGroup();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("×100 000").x + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.f);
        const integer warp = existent.warp;
        if (ImGui::BeginCombo("##universeWarp", fundamental::Existent::Always::warpLabel(warp))) {
            for (integer step = 0; step <= fundamental::Existent::Always::warpTop; ++step) {
                const bool selected = step == warp;
                if (ImGui::Selectable(fundamental::Existent::Always::warpLabel(step), selected))
                    setWarp(world, step);
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    void Game::drawMenuWindow(Writing world) {
        if (not ui.menu.has_value())
            return;
        bool open = true;
        if (ImGui::Begin("Menu", &open)) {
            if (ImGui::Button("Quit", ImVec2{140.f, 0.f})) {
                const auto bound = with<World>::get_global(world).window;
                if (bound and with<system::Window>::exists(world, *bound) and with<system::Device>::exists(world, *bound))
                    glfwSetWindowShouldClose(with<system::Device>::get(world, *bound).handle, true);
            }
        }
        ImGui::End();
        if (not open)
            ui.menu.reset();
    }

    void Game::contributeViewMenu(Writing world) {
        contributeChrome(world);
        if (uiMode == UiMode::locality) {
            ImGui::SameLine();
            if (ImGui::Button("Back"))
                closeLocalityScenario(world);
            contributeLocalityMenu(world);
            return;
        }
        if (starMap.menu.blueprints.has_value()) {
            ImGui::SameLine();
            if (ImGui::Button("Back"))
                starMap.menu.blueprints.reset();
            contributeEditorPanels(true);
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Planet"))
            openPlanetScenario(world);
        bool editor = false;
        rmmr::wrapper::ui::viewToggle("Blueprints", &editor);
        if (editor) {
            blueprints.openPanels();
            starMap.menu.blueprints.emplace();
        }
    }

    void Game::contributeEditorPanels(bool catalog) {
        if (catalog)
            togglePanel("Blueprints", blueprints.state.panels.catalog);
        togglePanel("Actions", blueprints.state.panels.actions);
        togglePanel("Clipboard", blueprints.state.panels.clipboard);
        togglePanel("Selection", blueprints.state.panels.selection);
        togglePanel("View", blueprints.state.panels.view);
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
        drawMenuWindow(world);
        if (uiMode == UiMode::locality) {
            drawLocalityUi(world);
            engageInputs(world);
            return;
        }
        if (starMap.menu.blueprints.has_value()) {
            blueprints.draw(world, true, blueprintPack, mountPack);
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
