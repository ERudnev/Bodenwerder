#include "demos/kubes/kubeOfKubes.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <imgui.h>
#include <string_view>
#include <vector>

#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics.q1.h>

namespace kubes {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        int compressedRaw(std::uint64_t raw) {
            std::uint64_t x = raw;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
            x = x ^ (x >> 31);
            return static_cast<int>(static_cast<std::uint32_t>(x));
        }

        template<typename Meta>
        void pushEntityId(const typename Meta::Id& id) {
            ImGui::PushID(compressedRaw(id.raw()));
        }

        auto passName(renderer::Pass pass) -> const char* {
            switch (pass) {
                case renderer::Pass::opaque: return "Opaque";
                case renderer::Pass::transparent: return "Transparent";
                case renderer::Pass::shadow: return "Shadow";
                case renderer::Pass::ui: return "UI";
                case renderer::Pass::gizmo: return "Gizmo";
                case renderer::Pass::sprite: return "Sprite";
                case renderer::Pass::identity: return "Identity";
            }
            return "Unknown";
        }

        auto displayName(const string& name) -> const char* {
            return name.empty() ? "(unnamed)" : name.c_str();
        }

        auto containsCaseInsensitive(std::string_view text, std::string_view needle) -> bool {
            if (needle.empty())
                return true;
            const auto it = std::search(
                text.begin(), text.end(),
                needle.begin(), needle.end(),
                [](char left, char right) {
                    return std::tolower(static_cast<unsigned char>(left))
                        == std::tolower(static_cast<unsigned char>(right));
                });
            return it != text.end();
        }

        auto filterText(const std::array<char, 128>& filter) -> std::string_view {
            return std::string_view(filter.data());
        }

        auto collectMaterials(Reading world, std::string_view filter) -> std::vector<resource::material::Asset::Id> {
            std::vector<resource::material::Asset::Id> materials;
            for (const auto entry : world->aspect<resource::material::Asset>().items()) {
                const auto& unit = with<resource::Unit>::get(world, entry.id);
                if (containsCaseInsensitive(unit.name, filter))
                    materials.push_back(entry.id);
            }
            std::sort(materials.begin(), materials.end(), [world](auto left, auto right) {
                const auto& left_unit = with<resource::Unit>::get(world, left);
                const auto& right_unit = with<resource::Unit>::get(world, right);
                if (left_unit.name != right_unit.name)
                    return left_unit.name < right_unit.name;
                return left.raw() < right.raw();
            });
            return materials;
        }

    } // namespace

    void KubeOfKubes::contributeViewMenu() {
        ImGui::MenuItem("Camera", nullptr, &ui.camera);
        ImGui::MenuItem("Lighting", nullptr, &ui.lighting);
        ImGui::MenuItem("Materials", nullptr, &ui.materials);
    }

    void KubeOfKubes::drawUi(Writing world) {
        drawCameraWindow(world);
        drawLightingWindow(world);
        drawMaterialsWindow(world);
    }

    void KubeOfKubes::drawCameraWindow(Writing world) {
        if (not ui.camera or views.empty())
            return;

        bool open = ui.camera;
        if (ImGui::Begin("Camera", &open)) {
            const auto camera = views.front().camera;
            if (not with<scene::Camera>::exists(world, camera)) {
                ImGui::TextDisabled("No camera selected.");
            } else {
                auto quantum = with<scene::Camera>::modify(world, camera);
                if (quantum->mode == scene::Camera::Mode::perspective) {
                    ImGui::SliderAngle("FoV", &quantum->fov_y, 10.0f, 160.0f);
                } else if (quantum->mode == scene::Camera::Mode::orthographic) {
                    ImGui::Text("Ortho size: %d x %d", quantum->ortho_size.x, quantum->ortho_size.y);
                } else {
                    ImGui::TextDisabled("Parallel projection (reserved).");
                }
                ImGui::DragFloat("Near", &quantum->z_near, 0.01f, 0.001f, quantum->z_far - 0.001f, "%.3f");
                ImGui::DragFloat("Far", &quantum->z_far, 0.1f, quantum->z_near + 0.001f, 1000.0f, "%.3f");
            }
        }
        ImGui::End();
        ui.camera = open;
    }

    void KubeOfKubes::drawLightingWindow(Writing world) {
        if (not ui.lighting or views.empty())
            return;

        bool open = ui.lighting;
        if (ImGui::Begin("Lighting", &open)) {
            const auto scene = views.front().scene;
            if (not with<scene::Root>::exists(world, scene)) {
                ImGui::TextDisabled("No scene selected.");
            } else {
                auto root = with<scene::Root>::modify(world, scene);
                ImGui::ColorEdit3("Ambient", &root->ambient.x);
                ImGui::DragFloat("Ambient intensity", &root->ambient_intensity, 0.05f, 0.0f, 20.0f, "%.2f");

                const auto& light_group = with<scene::Light_group>::get(world, scene);
                if (light_group.empty()) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Scene has no lights.");
                } else {
                    const auto light_id = *light_group.begin();
                    if (with<scene::Light>::exists(world, light_id)) {
                        ImGui::Separator();
                        ImGui::TextUnformatted("Primary light");
                        auto light = with<scene::Light>::modify(world, light_id);
                        ImGui::ColorEdit3("Color", &light->color.x);
                        ImGui::DragFloat("Intensity", &light->intensity, 0.05f, 0.0f, 100.0f, "%.2f");
                        ImGui::DragFloat("Range", &light->range, 0.1f, 0.0f, 500.0f, "%.2f");
                    }
                }
            }
        }
        ImGui::End();
        ui.lighting = open;
    }

    void KubeOfKubes::drawMaterialInspector(Writing world, resource::material::Asset::Id material_id) {
        auto material = with<resource::material::Asset>::modify(world, material_id);
        auto editable_unit = with<resource::Unit>::modify(world, material_id);
        auto& name_state = ui.material_name_edits[material_id.raw()];
        if (not name_state.editing)
            std::snprintf(name_state.buf.data(), name_state.buf.size(), "%s", editable_unit->name.c_str());

        ImGui::Text("Material #%llu", static_cast<unsigned long long>(material_id.raw()));
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("Name", name_state.buf.data(), name_state.buf.size()))
            editable_unit->name = string{name_state.buf.data()};
        name_state.editing = ImGui::IsItemActive();

        ImGui::Separator();
        ImGui::Text("Techniques: %zu", material->techniques.size());

        for (auto& [pass, technique] : material->techniques) {
            if (ImGui::CollapsingHeader(passName(pass), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (with<resource::Unit>::exists(world, technique.program.id)) {
                    const auto& shader_unit = with<resource::Unit>::get(world, technique.program.id);
                    ImGui::Text("Shader: %s", displayName(shader_unit.name));
                } else {
                    ImGui::TextColored(ImVec4(1.f, 0.25f, 0.25f, 1.f), "Shader: %s",
                        technique.program.backup.empty() ? "(missing)" : displayName(technique.program.backup));
                }

                if (technique.textures.empty()) {
                    ImGui::TextDisabled("No texture slots.");
                    continue;
                }

                for (auto& binding : technique.textures) {
                    pushEntityId<resource::material::Asset>(material_id);
                    ImGui::PushID(static_cast<int>(binding.uniform));

                    const string slot_label{material::Semantics::name_of(binding.uniform)};
                    const char* preview = "(missing)";
                    const bool texture_ok = with<resource::Unit>::exists(world, binding.texture.id);
                    if (texture_ok) {
                        const auto& texture_unit = with<resource::Unit>::get(world, binding.texture.id);
                        preview = displayName(texture_unit.name);
                    } else if (not binding.texture.backup.empty()) {
                        preview = displayName(binding.texture.backup);
                    }

                    if (not texture_ok)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.25f, 0.25f, 1.f));
                    if (ImGui::BeginCombo(slot_label.c_str(), preview)) {
                        for (const auto entry : world->aspect<resource::texture::Asset>().items()) {
                            pushEntityId<resource::texture::Asset>(entry.id);
                            const auto& texture_unit = with<resource::Unit>::get(world, entry.id);
                            const bool selected = entry.id == binding.texture.id;
                            if (ImGui::Selectable(displayName(texture_unit.name), selected))
                                binding.texture = resource::Unit::Actions::remember(world, entry.id);
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                    if (not texture_ok)
                        ImGui::PopStyleColor();

                    ImGui::PopID();
                    ImGui::PopID();
                }
            }
        }
    }

    void KubeOfKubes::drawMaterialsWindow(Writing world) {
        if (not ui.materials)
            return;

        bool open = ui.materials;
        if (ImGui::Begin("Materials", &open)) {
            const auto materials = collectMaterials(world, filterText(ui.material_filter));
            if (materials.empty()) {
                ui.selected_material.reset();
            } else if (not ui.selected_material.exists()
                or std::find(materials.begin(), materials.end(), *ui.selected_material) == materials.end()) {
                ui.selected_material = materials.front();
            }

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##materialFilter", "Filter materials...",
                ui.material_filter.data(), ui.material_filter.size());
            ImGui::Separator();

            ImGui::BeginChild("materialList", ImVec2{260.0f, 0.0f}, true);
            if (materials.empty()) {
                ImGui::TextDisabled("No materials match the filter.");
            } else {
                for (const auto material_id : materials) {
                    pushEntityId<resource::material::Asset>(material_id);
                    const auto& unit = with<resource::Unit>::get(world, material_id);
                    const bool selected = ui.selected_material.exists() and *ui.selected_material == material_id;
                    if (ImGui::Selectable(displayName(unit.name), selected))
                        ui.selected_material = material_id;
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("materialInspector", ImVec2{0.0f, 0.0f}, true);
            if (ui.selected_material.exists()
                and with<resource::material::Asset>::exists(world, *ui.selected_material)) {
                drawMaterialInspector(world, *ui.selected_material);
            } else {
                ImGui::TextDisabled("Select a material to inspect it.");
            }
            ImGui::EndChild();
        }
        ImGui::End();
        ui.materials = open;
    }

}
