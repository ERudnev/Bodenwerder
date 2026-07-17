#include "overlay.h"
#include "common.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <imgui.h>
#include <unordered_map>

#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/semantics.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/root.q1.h>

namespace rmmr::ui {

    using namespace fqsm::api;

    namespace {

        auto palette_contains(const resource::Uniform::Palette& palette, material::Semantics::Name semantic) -> bool {
            for (const auto uniform_id : palette) {
                if (uniform_id == material::Semantics::PersistentId{0}) {
                    continue;
                }
                if (material::Semantics::name_of(uniform_id) == semantic) {
                    return true;
                }
            }
            return false;
        }

        void draw_material_entry(fqsm::Writing world, scene::Root::Id scene, resource::material::Asset::Id material_id, const resource::material::Asset::Quantum& material) {
            push_entity_id<resource::material::Asset>(material_id);

            const auto& unit = with<resource::Unit>::get(world, material_id);
            const auto header = unit.name.empty() ? "Material" : unit.name.c_str();
            if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
                struct NameEditState {
                    std::array<char, 256> buf{};
                    bool editing = false;
                };
                static std::unordered_map<std::uint64_t, NameEditState> name_states;

                auto editable_unit = with<resource::Unit>::modify(world, material_id);
                auto& name_state = name_states[material_id.raw()];
                if (not name_state.editing) {
                    std::snprintf(name_state.buf.data(), name_state.buf.size(), "%s", editable_unit->name.c_str());
                }
                if (ImGui::InputText("Name", name_state.buf.data(), name_state.buf.size())) {
                    editable_unit->name = string{name_state.buf.data()};
                }
                name_state.editing = ImGui::IsItemActive();

                if (palette_contains(material.uniforms, "ambientColor") && with<scene::Root>::exists(world, scene)) {
                    auto root = with<scene::Root>::modify(world, scene);
                    ImGui::ColorEdit3("Ambient", &root->ambient.x);
                }

                if (palette_contains(material.uniforms, "light0Color") && with<scene::Root>::exists(world, scene)) {
                    const auto& light_group = with<scene::Light_group>::get(world, scene);
                    if (not light_group.empty()) {
                        const auto light_id = *light_group.begin();
                        if (with<scene::Light>::exists(world, light_id)) {
                            auto light = with<scene::Light>::modify(world, light_id);
                            ImGui::ColorEdit3("Light", &light->color.x);
                        }
                    }
                }
            }

            ImGui::PopID();
        }

    } // namespace

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

    /*
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
                    push_entity_id<scene::Node>(node);
                    auto actor = with<scene::PrimitiveActor>::modify(args.world, node);
                    ImGui::ColorPicker3("##color", &actor->albedo.x);
                    ImGui::PopID();
                    ++cube_index;
                }
                ImGui::EndTable();
            }
        }

        ImGui::End();
    }*/

    void draw_materials(FrameContext args) {
        if (not with<scene::Root>::exists(args.world, args.scene)) {
            return;
        }

        if (ImGui::Begin("Material View")) {
            if (with<resource::material::Asset>::count(args.world) == 0) {
                ImGui::TextDisabled("No material assets");
            } else {
                for (const auto entry : args.world->aspect<resource::material::Asset>().items()) {
                    draw_material_entry(args.world, args.scene, entry.id, entry.value);
                }
            }
        }
        ImGui::End();
    }

}
