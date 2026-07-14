#include "overlay.h"

#include <cstdint>
#include <imgui.h>

#include <rmmr/scene/actor.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>

namespace rmmr::ui {

    using namespace fqsm::api;

    void draw_camera(FrameContext args) {
        if (not with<scene::Camera>::exists(args.world, args.camera)) {
            return;
        }

        if (ImGui::Begin("Camera")) {
            auto camera = with<scene::Camera>::modify(args.world, args.camera);
            ImGui::SliderAngle("FoV", &camera->fov_y, 10.0f, 160.0f);
        }
        ImGui::End();
    }

    void draw_cubes(FrameContext args) {
        if (not with<scene::Root>::exists(args.world, args.scene)) {
            return;
        }

        if (ImGui::Begin("Cubes")) {

            if (ImGui::BeginTable("cubes", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2{0.0f, 0.0f})) {
                ImGui::TableSetupColumn("Cube");
                ImGui::TableSetupColumn("Color");
                ImGui::TableHeadersRow();

                const auto& node_group = with<scene::Node_group>::get(args.world, args.scene);
                integer cube_index = 0;
                for (const auto node : node_group) {
                    if (not with<scene::PrimitiveActor>::exists(args.world, node)) {
                        continue;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("Cube %d", static_cast<int>(cube_index));

                    ImGui::TableNextColumn();
                    ImGui::PushID(reinterpret_cast<const void*>(static_cast<std::intptr_t>(node.raw())));
                    auto actor = with<scene::PrimitiveActor>::modify(args.world, node);
                    ImGui::ColorPicker3("##color", &actor->albedo.x);
                    ImGui::PopID();
                    ++cube_index;
                }

                ImGui::EndTable();
            }
        }

        ImGui::End();
    }

}
