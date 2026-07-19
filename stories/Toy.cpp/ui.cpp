#include "ui.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <imgui.h>
#include <unordered_map>

#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics.q1.h>

namespace toy::ui {

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

        void drawSceneLighting(Writing world, scene::Root::Id scene) {
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

        void drawMaterialTextureSlots(Writing world, resource::material::Asset::Id material_id) {
            auto material = with<resource::material::Asset>::modify(world, material_id);
            integer slot = 0;
            for (auto& [_, technique] : material->techniques) {
                for (auto& binding : technique.textures) {
                    ImGui::PushID(static_cast<int>(slot++));
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
                            pushEntityId<resource::texture::Asset>(entry.id);
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
        }

        void drawMaterialEntry(Writing world, resource::material::Asset::Id material_id) {
            pushEntityId<resource::material::Asset>(material_id);

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

                drawMaterialTextureSlots(world, material_id);
            }

            ImGui::PopID();
        }

    } // namespace

    void State::drawToggles(Writing) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2{viewport->WorkPos.x + 10.0f, viewport->WorkPos.y + 10.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.65f);
        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("##renderer_toggles", nullptr, flags)) {
            ImGui::Checkbox("Materials", &materials);
        }
        ImGui::End();
    }

    void State::drawCamera(Writing world) {
        if (not camera.exists() or not with<scene::Camera>::exists(world, *camera)) {
            return;
        }

        if (ImGui::Begin("Camera")) {
            auto quantum = with<scene::Camera>::modify(world, *camera);
            ImGui::SliderAngle("FoV", &quantum->fov_y, 10.0f, 160.0f);
        }
        ImGui::End();
    }

    void State::drawMaterials(Writing world) {
        if (not scene.exists() or not with<scene::Root>::exists(world, *scene)) {
            return;
        }

        if (ImGui::Begin("Material View")) {
            drawSceneLighting(world, *scene);

            if (with<resource::material::Asset>::count(world) == 0) {
                ImGui::TextDisabled("No material assets");
            } else {
                for (const auto entry : world->aspect<resource::material::Asset>().items()) {
                    drawMaterialEntry(world, entry.id);
                }
            }
        }
        ImGui::End();
    }

    void State::draw(Writing world) {
        if (not scene.exists() or not camera.exists())
            return;
        drawToggles(world);
        drawCamera(world);
        if (materials)
            drawMaterials(world);
    }

}
