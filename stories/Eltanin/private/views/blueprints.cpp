#include "views/blueprints.h"

#include "mech/semantics/levelOne.h"
#include "mech/semantics/space.h"

#include <eltanin/resources/assets.q1.h>
#include <fQSM/identifier.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/window.q1.h>

#include <imgui.h>

#include <filesystem>
#include <format>
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
        state.levelOne.clear();
        state.levelTwo.clear();

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
        for (const auto& actor : state.levelOne)
            destroyMeshActor(context, actor.id);
        for (const auto& actor : state.levelTwo)
            destroyMeshActor(context, actor.id);
        state.levelOne.clear();
        state.levelTwo.clear();
        state.selected = asset_id;

        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, asset_id).data;
        const auto pack_one = *with<Assets>::find<meshpack::Asset>(context, Unit::Actions::name("Eltanin", "levelOne"));

        for (const auto& cell : data.cells) {
            if (auto frame = spawnFromPack(context, pack_one, mech::levelOne::mesh(cell.shape), cell.pose, mech::layer::frame, RGB{1.0f, 1.0f, 1.0f}, 1.0f))
                state.levelOne.push_back(*frame);
            if (auto inner = spawnFromPack(context, pack_one, mech::levelOne::innerMesh(cell.shape), cell.pose, mech::layer::inner, mech::slot::color(cell.role), 0.45f))
                state.levelOne.push_back(*inner);
        }
        for (const auto& stub : data.stubs) {
            if (auto wing = spawnFromPack(context, pack_one, mech::levelOne::mesh(stub.shape), stub.pose, mech::layer::wing, RGB{1.0f, 1.0f, 1.0f}, 1.0f))
                state.levelOne.push_back(*wing);
        }
        for (const auto& plate : data.hull) {
            if (auto a = spawnFromPack(context, pack_one, mech::levelOne::mesh(plate.shape), plate.pose, mech::layer::plate, RGB{1.0f, 1.0f, 1.0f}, 1.0f))
                state.levelOne.push_back(*a);
        }

        applyLayers(context);
    }

    auto Blueprints::spawnFromPack(Writing context, meshpack::Asset::Id pack, const std::string& entry, const mech::Pose& pose, mech::layer layer, RGB albedo, float opacity) -> base::maybe<Actor> {
        if (entry.empty())
            return {};
        const auto resolved = meshpack::Asset::Actions::resolve(context, pack, entry);
        if (not resolved)
            return {};
        const auto id = with<scene::Interface>::createMeshActor(
            context,
            *state.scene,
            scenePose(pose),
            scene::actor::Mesh::Quantum{
                .geometry = resolved->geometry,
                .materials = resolved->materials,
                .albedo = albedo,
                .scale = vec3{1.0f, 1.0f, 1.0f},
                .opacity = opacity,
                .visible = true,
            });
        scene::actor::Identified::Actions::extend(context, id);
        return Actor{.id = id, .layer = layer};
    }

    void Blueprints::applyLayers(Writing context) {
        for (const auto& actor : state.levelOne)
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
                ImGui::Text("L1 actors: %zu", state.levelOne.size());
                ImGui::Text("L2 actors: %zu", state.levelTwo.size());
                ImGui::Separator();
                ImGui::TextUnformatted("Under cursor");
                {
                    renderer::Integer32 under = renderer::Integer32{0};
                    for (const auto [_, window] : context->aspect<system::Window>().items()) {
                        under = window.current.under;
                        break;
                    }
                    if (under == renderer::Integer32{0}) {
                        ImGui::TextDisabled("—");
                    } else {
                        const auto label = std::format("#{}", fqsm::internal::id::info_hash(static_cast<fqsm::internal::id::BaseType>(under)));
                        ImGui::TextUnformatted(label.c_str());
                    }
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Layers (levelOne)");
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
