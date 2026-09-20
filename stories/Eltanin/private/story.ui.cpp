#include "story.h"

#include "mech/assembler.h"

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
#include <eltanin/locality/thing.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/controller/camera3d.q1.h>
#include "physics/settings.h"
#include "geo/celestial/horizon.h"
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/wrapper/ui.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

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

        constexpr float spaceScales[5] = {0.1f, 1.0f, 10.0f, 100.0f, 1000.0f};

        auto spaceScaleIndex(float scale) -> int {
            int picked = 1;
            for (int index = 0; index < 5; ++index)
                if (scale == spaceScales[index])
                    picked = index;
            return picked;
        }

        auto passName(renderer::Pass pass) -> const char* {
            switch (pass) {
                case renderer::Pass::opaque: return "Opaque";
                case renderer::Pass::transparent: return "Transparent";
                case renderer::Pass::shadow: return "Shadow";
                case renderer::Pass::gizmo: return "Gizmo";
                case renderer::Pass::sprite: return "Sprite";
                case renderer::Pass::environment: return "Environment";
                case renderer::Pass::atmosphere: return "Atmosphere";
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
        togglePanel("Inspector", ui.inspector);
        togglePanel("Space", ui.space);
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
        drawInspectorWindow(world);
        drawSpaceWindow(world);
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
        if (ImGui::RadioButton("Manual pos", not panel.spawnAtCamera))
            panel.spawnAtCamera = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("At camera", panel.spawnAtCamera))
            panel.spawnAtCamera = true;
        if (not panel.spawnAtCamera) {
            ImGui::DragFloat3("Spawn pos", &panel.spawnPos.x, 0.1f, 0.0f, 0.0f, "%.2f");
        } else if (world_view.has_value() and with<scene::Camera>::exists(world, world_view->camera)) {
            const auto& cameraPose = with<scene::Node>::get(world, world_view->camera).pose;
            ImGui::TextDisabled("Spawn pos: camera (%.2f, %.2f, %.2f)", cameraPose.position.x, cameraPose.position.y, cameraPose.position.z);
        } else {
            ImGui::TextDisabled("Spawn pos: camera (no view)");
        }
        ImGui::DragFloat3("Spawn HPB", &panel.spawnHpb.x, 0.1f, -180.0f, 180.0f, "%.1f°");
        ImGui::DragFloat3("Spawn vel", &panel.spawnVel.x, 0.1f, 0.0f, 0.0f, "%.2f");
        if (panel.blueprint.has_value() and not with<::eltanin::mech::Blueprint>::exists(world, *panel.blueprint))
            panel.blueprint = {};
        const char* preview = "(none)";
        if (panel.blueprint.has_value()) {
            const auto& asset = with<::eltanin::mech::Blueprint>::get(world, *panel.blueprint);
            const auto& unit = with<::rmmr::resource::Unit>::get(world, *panel.blueprint);
            preview = asset.name.empty() ? unit.name.own.c_str() : asset.name.c_str();
        }
        const auto pickShelf = [&](const char* heading, const BlueprintIds& ids) {
            ImGui::TextDisabled("%s", heading);
            if (ids.empty()) {
                ImGui::TextDisabled("(empty)");
                return;
            }
            for (const auto id : ids) {
                if (not with<::eltanin::mech::Blueprint>::exists(world, id))
                    continue;
                const auto& asset = with<::eltanin::mech::Blueprint>::get(world, id);
                const auto& unit = with<::rmmr::resource::Unit>::get(world, id);
                const char* label = asset.name.empty() ? unit.name.own.c_str() : asset.name.c_str();
                const bool selected = panel.blueprint.has_value() and *panel.blueprint == id;
                pushEntityId<::eltanin::mech::Blueprint>(id);
                if (ImGui::Selectable(label, selected))
                    panel.blueprint = id;
                if (selected)
                    ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
        };
        if (ImGui::BeginCombo("Blueprint", preview)) {
            pickShelf("Ships", blueprintPack.ships);
            ImGui::Separator();
            pickShelf("Prefabs", blueprintPack.prefabs);
            ImGui::EndCombo();
        }
        const bool cameraReady = not panel.spawnAtCamera or (world_view.has_value() and with<scene::Camera>::exists(world, world_view->camera));
        const bool canCreate = panel.blueprint.has_value() and world_view.has_value() and cameraReady;
        if (not canCreate)
            ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2{-1.0f, 0.0f}) and canCreate) {
            const Pos spawnPos = panel.spawnAtCamera ? with<scene::Node>::get(world, world_view->camera).pose.position : panel.spawnPos;
            mech::Assembler::spawn(world, Pose::from(spawnPos, panel.spawnHpb), *panel.blueprint, panel.spawnVel);
        }
        if (not canCreate)
            ImGui::EndDisabled();
        ImGui::End();
    }

    void Game::drawInspectorWindow(Writing world) {
        if (not ui.inspector.has_value())
            return;

        bool open = true;
        if (ImGui::Begin("Inspector", &open)) {
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (not cameras) {
                    ImGui::TextDisabled("Cameras are not ready.");
                } else {
                    const int mode = cameras->kind == Cameras::Kind::spectator ? 1 : 0;
                    const char* labels[] = { "Free", "Spectator" };
                    const bool canSpectator = not focus.things.empty();
                    if (ImGui::BeginCombo("Mode", labels[mode])) {
                        if (ImGui::Selectable("Free", mode == 0))
                            setCameraKind(world, Cameras::Kind::free);
                        if (not canSpectator)
                            ImGui::BeginDisabled();
                        if (ImGui::Selectable("Spectator", mode == 1) and canSpectator)
                            setCameraKind(world, Cameras::Kind::spectator);
                        if (not canSpectator)
                            ImGui::EndDisabled();
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("V");
                    if (not canSpectator)
                        ImGui::TextDisabled("Spectator needs a Focus.");

                    const auto camera = with<World>::get_global(world).camera;
                    if (not camera or not with<scene::Camera>::exists(world, *camera)) {
                        ImGui::TextDisabled("No camera selected.");
                    } else {
                        const auto& node = with<scene::Node>::get(world, *camera);
                        ImGui::Text("Pos: %.2f, %.2f, %.2f", node.pose.position.x, node.pose.position.y, node.pose.position.z);
                        if (planet and planet->well and with<phys::Body>::exists(world, *planet->well)) {
                            ImGui::Separator();
                            ImGui::TextUnformatted("Planet");
                            const auto& planetBody = with<phys::Body>::get(world, *planet->well);
                            const Pos cameraPos = node.pose.position;
                            const vec3 local = vec3{glm::inverse(glm::dquat{planetBody.orientation}) * (dvec3{cameraPos} - planetBody.position)};
                            const float range = glm::length(local);
                            const float altitude = planet->altitudeAt(planetBody, dvec3{cameraPos});
                            const float gravity = float(glm::length(planet->gravityAt(planetBody, dvec3{cameraPos})));
                            const float latDeg = range > 1.0e-3f ? glm::degrees(std::asin(glm::clamp(local.y / range, -1.0f, 1.0f))) : 0.0f;
                            const float lonDeg = range > 1.0e-3f ? glm::degrees(std::atan2(local.x, local.z)) : 0.0f;
                            ImGui::Text("Altitude: %.1f m", altitude);
                            ImGui::Text("g: %.3f m/s²", gravity);
                            const float air = planet->airDensity(planetBody, dvec3{cameraPos});
                            ImGui::Text("Air: %.0f g/m³ (%.0f%% ISA)", air, 100.0f * air / phys::Settings::Air::isaDensity);
                            ImGui::Text("Range to center: %.1f m (%.2f km)", range, range * 0.001f);
                            ImGui::Text("Lat / Lon: %.3f°, %.3f°", latDeg, lonDeg);
                            const auto hit = planet->probe(planetBody, local);
                            ImGui::Separator();
                            ImGui::TextUnformatted("Probe");
                            ImGui::Text("height %.2f m", hit.height);
                            ImGui::Text("position %.2f, %.2f, %.2f", hit.position.x, hit.position.y, hit.position.z);
                            ImGui::Text("normal %.3f, %.3f, %.3f", hit.normal.x, hit.normal.y, hit.normal.z);
                            ImGui::Text("slope %.3f", hit.slope);
                            ImGui::Text("mix %016llx", static_cast<unsigned long long>(hit.mix));
                        }
                    }

                    ImGui::Separator();
                    ImGui::TextUnformatted("Free");
                    if (not with<scene::Camera>::exists(world, cameras->free)) {
                        ImGui::TextDisabled("Free camera missing.");
                    } else {
                        auto node = with<scene::Node>::modify(world, cameras->free);
                        HPB hpb = node->pose.hpb();
                        if (ImGui::DragFloat3("HPB", &hpb.x, 0.1f, -180.0f, 180.0f, "%.1f°"))
                            node->pose.hpb(hpb);
                        auto quantum = with<scene::Camera>::modify(world, cameras->free);
                        if (quantum->mode == scene::Camera::Mode::perspective) {
                            ImGui::SliderAngle("FoV H", &quantum->fov_x, 10.0f, 160.0f);
                        } else if (quantum->mode == scene::Camera::Mode::orthographic) {
                            ImGui::Text("Ortho size: %d x %d", quantum->ortho_size.x, quantum->ortho_size.y);
                        } else {
                            ImGui::TextDisabled("Parallel projection (reserved).");
                        }
                        ImGui::DragFloat("Near", &quantum->z_near, 0.1f, 0.5f, 1000.0f, "%.1f");
                        if (quantum->mode == scene::Camera::Mode::orthographic)
                            ImGui::DragFloat("Far", &quantum->z_far, 100.0f, quantum->z_near + 1.0f, geo::Horizon::far, "%.0f");
                        else
                            ImGui::TextUnformatted("Far infinite");
                    }
                }
            }
        }
        ImGui::End();
        if (not open)
            ui.inspector.reset();
    }

    void Game::drawSpaceWindow(Writing world) {
        if (not ui.space.has_value())
            return;

        bool open = true;
        if (ImGui::Begin("Space", &open)) {
            float current = 1.0f;
            const bool hasCamera = cameras.has_value() and with<controller::Camera3d>::exists(world, cameras->free);
            if (hasCamera)
                current = with<controller::Camera3d>::get(world, cameras->free).moveScale;
            else if (grid.has_value() and with<scene::actor::MeshState>::exists(world, *grid))
                current = with<scene::actor::MeshState>::get(world, *grid).scale.x;
            int scale = spaceScaleIndex(current);
            if (ImGui::Combo("Scale", &scale, "×0.1\0×1\0×10\0×100\0×1000\0")) {
                const float next = spaceScales[scale];
                if (hasCamera)
                    with<controller::Camera3d>::modify(world, cameras->free)->moveScale = next;
                if (grid.has_value() and with<scene::actor::MeshState>::exists(world, *grid)) {
                    auto mesh = with<scene::actor::MeshState>::modify(world, *grid);
                    auto gizmo = with<scene::Grid>::modify(world, *grid);
                    mesh->scale = vec3{next};
                    mesh->patternScale = 1.0f;
                    gizmo->patternScale = 1.0f;
                }
            }
            if (not grid.has_value() or not with<scene::Grid>::exists(world, *grid)) {
                ImGui::TextDisabled("No grid in scene.");
            } else {
                auto node = with<scene::Node>::modify(world, *grid);
                bool visible = node->visible;
                if (ImGui::Checkbox("Grid", &visible))
                    node->visible = visible;
                ImGui::DragFloat3("Position", &node->pose.position.x, 0.1f, 0.0f, 0.0f, "%.2f");
                HPB hpb = node->pose.hpb();
                if (ImGui::DragFloat3("HPB", &hpb.x, 0.1f, -180.0f, 180.0f, "%.1f°"))
                    node->pose.hpb(hpb);
                if (with<scene::actor::MeshState>::exists(world, *grid)) {
                    auto mesh = with<scene::actor::MeshState>::modify(world, *grid);
                    auto gizmo = with<scene::Grid>::modify(world, *grid);
                    ImGui::SliderFloat("Opacity", &mesh->opacity, 0.0f, 1.0f, "%.2f");
                    gizmo->opacity = mesh->opacity;
                }
            }
        }
        ImGui::End();
        if (not open)
            ui.space.reset();
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
