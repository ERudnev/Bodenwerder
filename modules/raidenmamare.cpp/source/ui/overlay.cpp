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
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/root.q1.h>

namespace rmmr::ui {

    using namespace fqsm::api;

    namespace {

        void draw_scene_lighting(fqsm::Writing world, scene::Root::Id scene) {
            if (ImGui::CollapsingHeader("Scene lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto root = with<scene::Root>::modify(world, scene);
                ImGui::ColorEdit3("Ambient", &root->ambient.x);

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

        void draw_material_texture_slots(fqsm::Writing world, resource::material::Asset::Id material_id) {
            auto material = with<resource::material::Asset>::modify(world, material_id);
            for (integer slot = 0; slot < static_cast<integer>(material->textures.size()); ++slot) {
                ImGui::PushID(static_cast<int>(slot));
                auto& binding = material->textures[static_cast<std::size_t>(slot)];
                const string slot_label{material::Semantics::name_of(binding.id)};

                const char* preview = "(missing)";
                if (with<resource::Unit>::exists(world, binding.texture)) {
                    const auto& texture_unit = with<resource::Unit>::get(world, binding.texture);
                    if (not texture_unit.name.empty()) {
                        preview = texture_unit.name.c_str();
                    }
                }

                if (ImGui::BeginCombo(slot_label.c_str(), preview)) {
                    for (const auto entry : world->aspect<resource::texture::Asset>().items()) {
                        push_entity_id<resource::texture::Asset>(entry.id);
                        const auto& texture_unit = with<resource::Unit>::get(world, entry.id);
                        const char* option = texture_unit.name.empty() ? "(unnamed)" : texture_unit.name.c_str();
                        const bool selected = entry.id == binding.texture;
                        if (ImGui::Selectable(option, selected)) {
                            binding.texture = entry.id;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }

                ImGui::PopID();
            }
        }

        void draw_material_entry(fqsm::Writing world, resource::material::Asset::Id material_id) {
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

                draw_material_texture_slots(world, material_id);
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
            draw_scene_lighting(args.world, args.scene);

            if (with<resource::material::Asset>::count(args.world) == 0) {
                ImGui::TextDisabled("No material assets");
            } else {
                for (const auto entry : args.world->aspect<resource::material::Asset>().items()) {
                    draw_material_entry(args.world, entry.id);
                }
            }
        }
        ImGui::End();
    }

}
