#pragma once

#include <fQSM/api/interface.h>

#include <rmmr/wrapper/product.h>

#include <imgui.h>

namespace rmmr::wrapper::ui {

    using namespace fqsm::api;

    // Sticky toolbar toggle. Always SameLine() — put a lead control in contributeViewMenu before toggles.
    inline auto viewToggle(const char* label, bool* open) -> bool {
        ImGui::SameLine();
        const bool pressed = *open;
        if (pressed) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        const bool clicked = ImGui::Button(label);
        if (pressed)
            ImGui::PopStyleColor(2);
        if (clicked)
            *open = !*open;
        return clicked;
    }

    struct State {
        int64 lastAbsolute;

        void draw(Writing, Product&);

    private:
        void drawViewToolbar(Writing, Product&);
        void drawStatsWindow(Writing);
    };

}
