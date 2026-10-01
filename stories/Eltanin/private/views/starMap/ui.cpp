#include "game.h"

#include <eltanin/cluster/thing.q1.h>
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
            warp = std::clamp(warp, integer{0}, World::Always::warpTop);
            auto session = with<World>::modify_global(world);
            session->warp = warp;
            session->paused = warp == 0;
        }

    }

    void Game::contributeMenuBar(Writing world) {
        const bool menuOpen = ui.menuOpen;
        if (menuOpen) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button("Menu"))
            ui.menuOpen = not ui.menuOpen;
        if (menuOpen)
            ImGui::PopStyleColor(2);

        const auto& clock = with<cluster::Thing>::get_global(world);
        const auto civil = cluster::Thing::Always::civil(clock.now);
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
        const integer warp = with<World>::get_global(world).warp;
        if (ImGui::BeginCombo("##sessionWarp", World::Always::warpLabel(warp))) {
            for (integer step = 0; step <= World::Always::warpTop; ++step) {
                const bool selected = step == warp;
                if (ImGui::Selectable(World::Always::warpLabel(step), selected))
                    setWarp(world, step);
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    void Game::drawMenuWindow(Writing world) {
        if (not ui.menuOpen)
            return;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2{viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.5f}, ImGuiCond_Always, ImVec2{0.5f, 0.5f});
        ImGui::SetNextWindowSize(ImVec2{156.f, 48.f}, ImGuiCond_Always);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##menu", nullptr, flags)) {
            if (ImGui::Button("Quit", ImVec2{140.f, 0.f})) {
                const auto bound = with<World>::get_global(world).window;
                if (bound and with<system::Window>::exists(world, *bound) and with<system::Device>::exists(world, *bound))
                    glfwSetWindowShouldClose(with<system::Device>::get(world, *bound).handle, true);
            }
        }
        ImGui::End();
    }

    void Game::contributeViewMenu(Writing world) {
        contributeMenuBar(world);
        if (view.shown == Shown::locality and view.locality) {
            ImGui::SameLine();
            if (ImGui::Button("Back"))
                showMap(world);
            if (shared)
                view.locality->contributeMenu(world, assets, *shared);
            return;
        }
        if (view.shown == Shown::editor) {
            ImGui::SameLine();
            if (ImGui::Button("Back"))
                showMap(world);
            contributeEditorPanels(true);
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Planet"))
            showLocality(world);
        togglePanel("Systems", view.starMap.panels.systems);
        togglePanel("Selected", view.starMap.panels.selected);
        togglePanel("View", view.starMap.panels.view);
        bool editorToggle = false;
        rmmr::wrapper::ui::viewToggle("Blueprints", &editorToggle);
        if (editorToggle)
            showEditor(world);
    }

    void Game::contributeEditorPanels(bool catalog) {
        if (catalog)
            togglePanel("Blueprints", view.editor.state.panels.catalog);
        togglePanel("Actions", view.editor.state.panels.actions);
        togglePanel("Clipboard", view.editor.state.panels.clipboard);
        togglePanel("Selection", view.editor.state.panels.selection);
        togglePanel("View", view.editor.state.panels.view);
    }

    auto Game::activeOverlay() const -> base::maybe<rmmr::resource::overlay::Asset::Id> {
        if (view.shown != Shown::editor or not view.editor.assets.editorEffect)
            return {};
        if (view.editor.state.membranes.enabled or view.editor.state.paletteMode)
            return {};
        return view.editor.assets.editorEffect;
    }

    auto Game::overlaySelection() const -> std::span<const rmmr::renderer::Integer32> {
        if (view.shown != Shown::editor or view.editor.state.membranes.enabled or view.editor.state.paletteMode)
            return {};
        return view.editor.state.selection.aliases;
    }

    void Game::drawUi(Writing world) {
        drawMenuWindow(world);
        if (view.shown == Shown::locality and view.locality) {
            view.locality->draw(world, entities.blueprintPack);
            view.locality->present(views);
            engageInputs(world);
            return;
        }
        if (view.shown == Shown::editor) {
            view.editor.draw(world, true, entities.blueprintPack, entities.mountPack);
            if (view.starMap.view)
                view.editor.bindView(views, true, *view.starMap.view);
            engageInputs(world);
            return;
        }
        if (view.starMap.view)
            views = {*view.starMap.view};
        engageInputs(world);
        view.starMap.draw(world);

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        constexpr float pad = 10.0f;
        ImGui::SetNextWindowPos(ImVec2{viewport->WorkPos.x + viewport->WorkSize.x - pad, viewport->WorkPos.y + viewport->WorkSize.y - pad}, ImGuiCond_Always, ImVec2{1.0f, 1.0f});
        ImGui::SetNextWindowBgAlpha(0.48f);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{8.0f, 2.0f});
        if (ImGui::Begin("##starmapReadout", nullptr, flags)) {
            ImGui::SetWindowFontScale(0.75f);
            const auto& visuals = view.starMap.visuals;
            ImGui::Text("Scale   %.1f LY", visuals.scaleLy);
            ImGui::Text("Player  %.2f, %.2f, %.2f LY", visuals.player.x, visuals.player.y, visuals.player.z);
            ImGui::Text("Focus   %.2f, %.2f, %.2f LY", visuals.focus.x, visuals.focus.y, visuals.focus.z);
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

}
