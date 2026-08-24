#include "story.h"

#include "assembler.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <string_view>
#include <vector>

#include <base/logging.h>
#include <eltanin/mech/blueprint.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/wrapper/ui.h>

namespace eltanin {

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

        auto passName(renderer::Pass pass) -> const char* {
            switch (pass) {
                case renderer::Pass::opaque: return "Opaque";
                case renderer::Pass::transparent: return "Transparent";
                case renderer::Pass::shadow: return "Shadow";
                case renderer::Pass::gizmo: return "Gizmo";
                case renderer::Pass::sprite: return "Sprite";
                case renderer::Pass::environment: return "Environment";
                case renderer::Pass::identitySelected: return "IdentitySelected";
                case renderer::Pass::identity: return "Identity";
            }
            return "Unknown";
        }

        auto displayName(const ::rmmr::resource::Unit::Name& name) -> const char* {
            thread_local string buffer;
            if (name.empty())
                return "(unnamed)";
            buffer = name.text();
            return buffer.c_str();
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

        auto collectMaterials(Reading world, std::string_view filter) -> std::vector<::rmmr::resource::material::Asset::Id> {
            std::vector<::rmmr::resource::material::Asset::Id> materials;
            for (const auto entry : world->aspect<::rmmr::resource::material::Asset>().items()) {
                const auto& unit = with<::rmmr::resource::Unit>::get(world, entry.id);
                if (containsCaseInsensitive(unit.name.text(), filter))
                    materials.push_back(entry.id);
            }
            std::sort(materials.begin(), materials.end(), [world](auto left, auto right) {
                const auto& left_unit = with<::rmmr::resource::Unit>::get(world, left);
                const auto& right_unit = with<::rmmr::resource::Unit>::get(world, right);
                if (left_unit.name != right_unit.name)
                    return left_unit.name < right_unit.name;
                return left.raw() < right.raw();
            });
            return materials;
        }

    } // namespace

    void Game::contributeViewMenu(Writing world) {
        bool paused = with<World>::get_global(world).paused;
        rmmr::wrapper::ui::viewToggle("Pause", &paused);
        if (paused != with<World>::get_global(world).paused)
            with<World>::modify_global(world)->paused = paused;
        togglePanel("Camera", ui.camera);
        togglePanel("Lighting", ui.lighting);
        togglePanel("Materials", ui.materials);
        {
            bool open = ui.physics.has_value();
            rmmr::wrapper::ui::viewToggle("Physics", &open);
            if (open != ui.physics.has_value()) {
                if (open) {
                    if (not assets.collisionDebugMaterial or not shared->material.gizmo.vertexColor or not shared->texture.debug or not assets.primitive.diamond or not assets.primitive.sphere) {
                        base::message("eltanin::Game: Physics UI needs collision debug material, gizmo vertex color, debug texpack, diamond and sphere primitives");
                    } else {
                        ui.physics.emplace(*assets.primitive.diamond, *assets.primitive.sphere, *shared->material.gizmo.vertexColor, *assets.collisionDebugMaterial, *shared->texture.debug);
                    }
                } else {
                    ui.physics.reset();
                }
            }
        }
        togglePanel("Blueprints", ui.blueprints);
    }

    auto Game::activeOverlay() const -> base::maybe<rmmr::resource::overlay::Asset::Id> {
        if (not ui.blueprints.has_value() or not assets.blueprintsEditorEffect)
            return {};
        // Membrane tile place / mount palette: suppress hover/selection chrome. F3 keeps it for mounts.
        if (blueprints.state.membranes.enabled or blueprints.state.paletteMode)
            return {};
        return assets.blueprintsEditorEffect;
    }

    auto Game::overlaySelection() const -> std::span<const rmmr::renderer::Integer32> {
        if (not ui.blueprints.has_value() or blueprints.state.membranes.enabled or blueprints.state.paletteMode)
            return {};
        return blueprints.state.selection.aliases;
    }

    void Game::drawUi(Writing world) {
        drawCameraWindow(world);
        drawLightingWindow(world);
        drawMaterialsWindow(world);
        drawAssemblerWindow(world);
        if (ui.physics.has_value() and physics.has_value()) {
            bool open = true;
            ui.physics->draw(world, open, *physics);
            if (not open)
                ui.physics.reset();
        }
        if (ui.blueprints.has_value()) {
            bool open = true;
            blueprints.draw(world, open, blueprintPack, mountPack);
            if (not open)
                ui.blueprints.reset();
        }
        if (world_view)
            blueprints.bindView(views, ui.blueprints.has_value(), *world_view);
    }

    void Game::drawAssemblerWindow(Writing world) {
        if (not ImGui::Begin("Assembler")) {
            ImGui::End();
            return;
        }
        auto& panel = ui.assembler;
        ImGui::DragFloat3("Spawn pos", &panel.spawnPos.x, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::DragFloat3("Spawn HPB", &panel.spawnHpb.x, 0.1f, -180.0f, 180.0f, "%.1f°");
        if (panel.ship.has_value() and not with<::eltanin::mech::Blueprint>::exists(world, *panel.ship))
            panel.ship = {};
        const char* preview = "(none)";
        if (panel.ship.has_value()) {
            const auto& asset = with<::eltanin::mech::Blueprint>::get(world, *panel.ship);
            const auto& unit = with<::rmmr::resource::Unit>::get(world, *panel.ship);
            preview = asset.name.empty() ? unit.name.own.c_str() : asset.name.c_str();
        }
        if (ImGui::BeginCombo("Ship", preview)) {
            for (const auto id : blueprintPack.ships) {
                if (not with<::eltanin::mech::Blueprint>::exists(world, id))
                    continue;
                const auto& asset = with<::eltanin::mech::Blueprint>::get(world, id);
                const auto& unit = with<::rmmr::resource::Unit>::get(world, id);
                const char* label = asset.name.empty() ? unit.name.own.c_str() : asset.name.c_str();
                const bool selected = panel.ship.has_value() and *panel.ship == id;
                pushEntityId<::eltanin::mech::Blueprint>(id);
                if (ImGui::Selectable(label, selected))
                    panel.ship = id;
                if (selected)
                    ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        const bool canCreate = panel.ship.has_value();
        if (not canCreate)
            ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2{-1.0f, 0.0f}) and canCreate)
            Assembler::immediateSpawn(world, *panel.ship, Pose::from(panel.spawnPos, panel.spawnHpb));
        if (not canCreate)
            ImGui::EndDisabled();
        ImGui::End();
    }

    void Game::drawCameraWindow(Writing world) {
        if (not ui.camera.has_value() or views.empty())
            return;

        bool open = true;
        if (ImGui::Begin("Camera", &open)) {
            const auto& view = views.front();
            const auto camera = view.camera;
            if (not with<scene::Camera>::exists(world, camera)) {
                ImGui::TextDisabled("No camera selected.");
            } else {
                const auto& node = with<scene::Node>::get(world, camera);
                ImGui::Text("Pos: %.2f, %.2f, %.2f", node.pose.position.x, node.pose.position.y, node.pose.position.z);

                HPB hpb = node.pose.hpb();
                if (ImGui::DragFloat3("HPB", &hpb.x, 0.1f, -180.0f, 180.0f, "%.1f°"))
                    with<scene::Node>::modify(world, camera)->pose.hpb(hpb);

                auto quantum = with<scene::Camera>::modify(world, camera);
                if (quantum->mode == scene::Camera::Mode::perspective) {
                    ImGui::SliderAngle("FoV H", &quantum->fov_x, 10.0f, 160.0f);
                } else if (quantum->mode == scene::Camera::Mode::orthographic) {
                    ImGui::Text("Ortho size: %d x %d", quantum->ortho_size.x, quantum->ortho_size.y);
                } else {
                    ImGui::TextDisabled("Parallel projection (reserved).");
                }
                ImGui::DragFloat("Near", &quantum->z_near, 0.1f, 1.0f, quantum->z_far - 1.0f, "%.1f");
                ImGui::DragFloat("Far", &quantum->z_far, 16.0f, quantum->z_near + 1.0f, 32768.0f, "%.0f");
            }
        }
        ImGui::End();
        if (not open)
            ui.camera.reset();
    }

    void Game::drawLightingWindow(Writing world) {
        if (not ui.lighting.has_value() or views.empty())
            return;

        bool open = true;
        if (ImGui::Begin("Lighting", &open)) {
            const auto scene = views.front().scene;
            if (not with<scene::Root>::exists(world, scene)) {
                ImGui::TextDisabled("No scene selected.");
            } else {
                auto root = with<scene::Root>::modify(world, scene);
                ImGui::ColorEdit3("Ambient", &root->ambient.x);
                ImGui::DragFloat("Ambient intensity", &root->ambient_intensity, 0.05f, 0.0f, 20.0f, "%.2f");
                ImGui::Separator();
                ImGui::TextUnformatted("Bloom");
                ImGui::DragFloat("Bloom radius", &root->bloom.radius, 0.05f, 0.0f, 8.0f, "%.2f");
                ImGui::DragFloat("Bloom intensity", &root->bloom.intensity, 0.05f, 0.0f, 8.0f, "%.2f");
                ImGui::DragFloat3("Gravity", &root->gravity.x, 0.01f, 0.0f, 0.0f, "%.3f");
                ImGui::DragFloat("Atmosphere density", &root->atmosphereDensity, 1.0f, 0.0f, 0.0f, "%.0f g/m³");

                if (not root->primaryLight) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Scene has no primary light.");
                } else {
                    const auto light_id = *root->primaryLight;
                    if (with<scene::Light>::exists(world, light_id)) {
                        ImGui::Separator();
                        ImGui::TextUnformatted("Primary light");
                        auto light = with<scene::Light>::modify(world, light_id);
                        auto node = with<scene::Node>::modify(world, light_id);
                        if (light->kind == scene::Light::Kind::directional) {
                            HPB sun = node->pose.hpb();
                            if (ImGui::DragFloat3("Sun HPB", &sun.x, 0.5f, 0.0f, 0.0f, "%.1f"))
                                node->pose.hpb(sun);
                        } else {
                            ImGui::DragFloat3("Position", &node->pose.position.x, 0.1f, 0.0f, 0.0f, "%.2f");
                            ImGui::DragFloat("Range", &light->range, 0.1f, 0.0f, 500.0f, "%.2f");
                        }
                        ImGui::ColorEdit3("Color", &light->color.x);
                        ImGui::DragFloat("Intensity", &light->intensity, 0.05f, 0.0f, 100.0f, "%.2f");
                    }
                }
            }
        }
        ImGui::End();
        if (not open)
            ui.lighting.reset();
    }

    void Game::drawMaterialInspector(Writing world, ::rmmr::resource::material::Asset::Id material_id) {
        auto material = with<::rmmr::resource::material::Asset>::modify(world, material_id);
        auto editable_unit = with<::rmmr::resource::Unit>::modify(world, material_id);
        auto& nameEdits = ui.materials->nameEdits;
        auto found = nameEdits.find(material_id);
        if (found == nameEdits.end())
            found = nameEdits.emplace(material_id, Ui::Materials::NameEdit{.buf = {}, .editing = false}).first;
        auto& name_state = found->second;
        if (not name_state.editing)
            std::snprintf(name_state.buf.data(), name_state.buf.size(), "%s", editable_unit->name.own.c_str());

        ImGui::Text("Material #%llu", static_cast<unsigned long long>(material_id.raw()));
        ImGui::Text("Library: %s", editable_unit->name.library.empty() ? "(none)" : editable_unit->name.library.c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("Name", name_state.buf.data(), name_state.buf.size()))
            editable_unit->name.own = string{name_state.buf.data()};
        name_state.editing = ImGui::IsItemActive();

        ImGui::Separator();
        ImGui::Text("Techniques: %zu", material->techniques.size());

        for (auto& [pass, technique] : material->techniques) {
            if (ImGui::CollapsingHeader(passName(pass), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (with<::rmmr::resource::Unit>::exists(world, technique.program.id)) {
                    const auto& shader_unit = with<::rmmr::resource::Unit>::get(world, technique.program.id);
                    ImGui::Text("Shader: %s", displayName(shader_unit.name));
                } else {
                    ImGui::TextColored(ImVec4(1.f, 0.25f, 0.25f, 1.f), "Shader: %s",
                        technique.program.backup.empty() ? "(missing)" : displayName(technique.program.backup));
                }

                ImGui::TextDisabled("Sampler values are on the draw, not on the material.");
            }
        }
    }

    void Game::drawMaterialsWindow(Writing world) {
        if (not ui.materials.has_value())
            return;

        bool open = true;
        auto& panel = *ui.materials;
        if (ImGui::Begin("Materials", &open)) {
            const auto materials = collectMaterials(world, filterText(panel.filter));
            if (materials.empty()) {
                panel.selected.reset();
            } else if (not panel.selected.has_value()
                or std::find(materials.begin(), materials.end(), *panel.selected) == materials.end()) {
                panel.selected = materials.front();
            }

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##materialFilter", "Filter materials...",
                panel.filter.data(), panel.filter.size());
            ImGui::Separator();

            ImGui::BeginChild("materialList", ImVec2{260.0f, 0.0f}, true);
            if (materials.empty()) {
                ImGui::TextDisabled("No materials match the filter.");
            } else {
                for (const auto material_id : materials) {
                    pushEntityId<::rmmr::resource::material::Asset>(material_id);
                    const auto& unit = with<::rmmr::resource::Unit>::get(world, material_id);
                    const bool selected = panel.selected.has_value() and *panel.selected == material_id;
                    if (ImGui::Selectable(displayName(unit.name), selected))
                        panel.selected = material_id;
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("materialInspector", ImVec2{0.0f, 0.0f}, true);
            if (panel.selected.has_value()
                and with<::rmmr::resource::material::Asset>::exists(world, *panel.selected)) {
                drawMaterialInspector(world, *panel.selected);
            } else {
                ImGui::TextDisabled("Select a material to inspect it.");
            }
            ImGui::EndChild();
        }
        ImGui::End();
        if (not open)
            ui.materials.reset();
    }

}
