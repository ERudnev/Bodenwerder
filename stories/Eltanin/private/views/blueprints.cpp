#include "views/blueprints.h"

#include "mech/semantics/levelOne.h"
#include "mech/semantics/space.h"

#include <eltanin/resources/assets.q1.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/root.q1.h>

#include <imgui.h>

#include <filesystem>
#include <numbers>

namespace eltanin::views {

    using namespace rmmr;
    using namespace rmmr::resource;

    namespace {

        void destroyMeshActor(Writing context, scene::actor::Mesh::Id actor) {
            for (const auto [root, group] : context->aspect<scene::Node_group>().items()) {
                if (group.contains(actor)) {
                    with<scene::Node_group>::deleteElement(context, root, actor);
                    return;
                }
            }
            if (with<scene::Node>::exists(context, actor))
                with<scene::Node>::remove(context, actor);
        }

        auto layerVisible(const Blueprints::Layers& layers, mech::layer layer) -> bool {
            switch (layer) {
                case mech::layer::plate: return layers.plate;
                case mech::layer::frame: return layers.frame;
                case mech::layer::inner: return layers.inner;
                case mech::layer::wing: return layers.wing;
            }
            return false;
        }

        // Cell index × edge; ori → orient::matrix → quat. (toLocal is for corners inside a cell.)
        auto scenePose(const mech::Pose& pose) -> Pose {
            const float cell = mech::physical::edgeMeters;
            return Pose{
                .position = Pos{
                    static_cast<float>(pose.pos.x) * cell,
                    static_cast<float>(pose.pos.y) * cell,
                    static_cast<float>(pose.pos.z) * cell,
                },
                .rotation = glm::normalize(glm::quat_cast(glm::mat3(mech::orient::matrix[static_cast<std::size_t>(pose.ori)]))),
            };
        }

        // Bootstrap stand-in: unit kube + lit (Mesh parts optional — submit falls back to whole mesh).
        auto placeholderMesh(Reading context) -> base::maybe<scene::actor::Mesh::Quantum> {
            const auto geometry = with<Assets>::find<geometry::Asset>(context, Unit::Actions::name("rmmr", "kube"));
            const auto lit = with<Assets>::find<rmmr::resource::material::Asset>(context, Unit::Actions::name("rmmr", "lit"));
            if (not geometry or not lit)
                return {};
            const float cell = mech::physical::edgeMeters;
            return scene::actor::Mesh::Quantum{
                .geometry = *geometry,
                .materials = {{"mesh", *lit}},
                .albedo = RGB{1.0f, 1.0f, 1.0f},
                .scale = vec3{cell, cell, cell},
                .visible = true,
            };
        }

        auto spawnPlaceholder(Writing context, scene::Root::Id root, const mech::Pose& pose, mech::layer layer, const scene::actor::Mesh::Quantum& mesh) -> Blueprints::Actor {
            auto quantum = mesh;
            quantum.visible = true;
            const float cell = mech::physical::edgeMeters;
            switch (layer) {
                case mech::layer::plate:
                    quantum.albedo = RGB{0.75f, 0.78f, 0.82f};
                    quantum.scale = vec3{cell * 1.05f, cell * 0.08f, cell * 1.05f};
                    break;
                case mech::layer::frame:
                    // Prefer Blueprints::spawnFrame; leftover only if mis-routed.
                    quantum.albedo = RGB{0.35f, 0.55f, 0.95f};
                    quantum.scale = vec3{cell, cell, cell};
                    break;
                case mech::layer::inner:
                    quantum.albedo = RGB{0.95f, 0.75f, 0.25f};
                    quantum.scale = vec3{cell * 0.55f, cell * 0.55f, cell * 0.55f};
                    break;
                case mech::layer::wing:
                    quantum.albedo = RGB{0.45f, 0.85f, 0.55f};
                    quantum.scale = vec3{cell * 0.4f, cell * 0.15f, cell * 1.2f};
                    break;
            }
            const auto id = with<scene::Interface>::createMeshActor(context, root, scenePose(pose), std::move(quantum));
            return Blueprints::Actor{.id = id, .layer = layer};
        }

    } // namespace

    void Blueprints::create(Writing context, filepath directory) {
        const auto grid_name = Unit::Actions::name("rmmr", "grid");
        const auto grid_geometry = with<Assets>::find<geometry::Asset>(context, grid_name);
        const auto grid_material = with<Assets>::find<rmmr::resource::material::Asset>(context, grid_name);
        if (not grid_geometry or not grid_material)
            return (void)context.refuse("eltanin::views::Blueprints::create: rmmr::grid geometry/material missing");

        const auto root = with<scene::Interface>::createScene(context);

        const float cell = mech::physical::edgeMeters;
        const float pattern_scale = 1.0f / cell;
        constexpr int levels[3]{-1, 0, 1};
        for (std::size_t index = 0; index < state.grids.size(); ++index) {
            state.grids[index] = with<scene::Interface>::createGrid(
                context,
                root,
                Pose::from(Pos{0.0f, static_cast<float>(levels[index]) * cell, 0.0f}, HPB{0.0f, 0.0f, 0.0f}),
                item<scene::Grid>{.geometry = *grid_geometry, .material = *grid_material, .opacity = 0.35f, .pattern_scale = pattern_scale});
        }

        const Pos camera_pos{12.0f, 10.0f, 20.0f};
        const auto camera = with<scene::Interface>::createCamera(
            context,
            root,
            Pose::from(camera_pos, HPB{36.87f, -29.74f, 0.0f}),
            100.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::Camera3d>::create(context, camera);

        with<scene::Interface>::createLight(
            context,
            root,
            Pose::from(Pos{9.5f, 19.0f, 7.5f}, HPB{0.0f, 0.0f, 0.0f}),
            item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 60.0f});

        state.scene = root;
        state.camera = camera;
        state.loaded.clear();
        state.selected.reset();
        state.layers = Layers{.plate = true, .frame = true, .inner = true, .wing = true};
        state.actors.clear();

        if (not std::filesystem::is_directory(directory))
            return (void)context.refuse(std::format("eltanin::views::Blueprints::create: not a directory '{}'", directory.string()));

        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (not entry.is_regular_file() or entry.path().extension() != ".blueprint")
                continue;

            const auto stem = entry.path().stem().string();
            const auto relative = filename{(std::filesystem::path{"blueprints"} / entry.path().filename()).generic_string()};
            const auto asset_id = with<::eltanin::resource::Assets>::add_blueprint_loader(
                context,
                Unit::name("Eltanin", stem),
                item<::eltanin::resource::blueprint::Loader>{.file = relative});
            ::eltanin::resource::blueprint::Loader::Actions::load(context, asset_id);
            state.loaded.push_back(asset_id);
        }

        if (not state.loaded.empty())
            show(context, state.loaded.front());
    }

    void Blueprints::show(Writing context, resource::blueprint::Asset::Id asset_id) {
        for (const auto& actor : state.actors)
            destroyMeshActor(context, actor.id);
        state.actors.clear();
        state.selected = asset_id;

        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, asset_id).data;
        const auto root = *state.scene;
        const auto mesh = *placeholderMesh(context);
        const auto frames_pack = *with<Assets>::find<meshpack::Asset>(context, Unit::Actions::name("Eltanin", "levelTwo"));

        for (const auto& cell : data.cells) {
            if (auto frame = spawnFrame(context, cell, frames_pack))
                state.actors.push_back(*frame);
            state.actors.push_back(spawnPlaceholder(context, root, cell.pose, mech::layer::inner, mesh));
        }
        for (const auto& stub : data.stubs)
            state.actors.push_back(spawnPlaceholder(context, root, stub.pose, mech::layer::wing, mesh));
        for (const auto& plate : data.hull)
            state.actors.push_back(spawnPlaceholder(context, root, plate.pose, mech::layer::plate, mesh));

        applyLayers(context);
    }

    auto Blueprints::spawnFrame(Writing context, const mech::Element::Cell& cell, meshpack::Asset::Id pack) -> base::maybe<Actor> {
        const auto entry = mech::levelOne::mesh(cell.shape);
        if (entry.empty())
            return {};
        const auto resolved = meshpack::Asset::Actions::resolve(context, pack, entry);
        if (not resolved)
            return {};
        const auto id = with<scene::Interface>::createMeshActor(
            context,
            *state.scene,
            scenePose(cell.pose),
            scene::actor::Mesh::Quantum{
                .geometry = resolved->geometry,
                .materials = resolved->materials,
                .albedo = RGB{1.0f, 1.0f, 1.0f},
                .scale = vec3{1.0f, 1.0f, 1.0f},
                .visible = true,
            });
        return Actor{.id = id, .layer = mech::layer::frame};
    }

    void Blueprints::applyLayers(Writing context) {
        for (const auto& actor : state.actors)
            scene::actor::Mesh::Actions::setVisible(context, actor.id, layerVisible(state.layers, actor.layer));
    }

    void Blueprints::draw(Writing context, bool& open) {
        if (not open)
            return;

        bool shown = open;
        if (ImGui::Begin("Blueprints", &shown)) {
            ImGui::BeginChild("blueprintList", ImVec2{220.0f, 0.0f}, true);
            if (state.loaded.empty()) {
                ImGui::TextDisabled("No blueprints loaded.");
            } else {
                for (const auto asset_id : state.loaded) {
                    const auto& unit = with<Unit>::get(context, asset_id);
                    const auto& asset = with<::eltanin::resource::blueprint::Asset>::get(context, asset_id);
                    const bool selected = state.selected.exists() and *state.selected == asset_id;
                    const char* label = asset.data.name.empty() ? unit.name.own.c_str() : asset.data.name.c_str();
                    if (ImGui::Selectable(label, selected))
                        show(context, asset_id);
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("blueprintDetails", ImVec2{0.0f, 0.0f}, true);
            if (not state.selected.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.selected)) {
                ImGui::TextDisabled("Select a blueprint.");
            } else {
                const auto& unit = with<Unit>::get(context, *state.selected);
                const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, *state.selected).data;
                ImGui::Text("Unit: %s", unit.name.text().c_str());
                ImGui::Text("Name: %s", data.name.c_str());
                ImGui::Text("Author: %s", data.author.c_str());
                ImGui::Separator();
                ImGui::Text("Cells: %zu", data.cells.size());
                ImGui::Text("Stubs: %zu", data.stubs.size());
                ImGui::Text("Hull plates: %zu", data.hull.size());
                ImGui::Text("Actors: %zu", state.actors.size());
                ImGui::Separator();
                ImGui::TextUnformatted("Layers");
                bool layers_changed = false;
                layers_changed |= ImGui::Checkbox("plate", &state.layers.plate);
                layers_changed |= ImGui::Checkbox("frame", &state.layers.frame);
                layers_changed |= ImGui::Checkbox("inner", &state.layers.inner);
                layers_changed |= ImGui::Checkbox("wing", &state.layers.wing);
                if (layers_changed)
                    applyLayers(context);
            }
            ImGui::EndChild();
        }
        ImGui::End();
        open = shown;
    }

    void Blueprints::bindView(std::vector<rmmr::wrapper::Product::View>& product_views, bool open, const rmmr::wrapper::Product::View& world_view) const {
        if (not open or not state.scene or not state.camera) {
            product_views = {world_view};
            return;
        }
        product_views = {
            rmmr::wrapper::Product::View{
                .viewport = world_view.viewport,
                .scene = *state.scene,
                .camera = *state.camera,
            },
        };
    }

}
