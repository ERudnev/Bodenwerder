#include "views/blueprints.h"

#include "mech/semantics/levelOne.h"
#include "mech/semantics/shapes.h"
#include "mech/semantics/space.h"

#include <eltanin/resources/assets.q1.h>
#include <fQSM/identifier.h>
#include <rmmr/controller/cameraOrbit.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/window.q1.h>

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <numbers>
#include <set>
#include <utility>

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
                case mech::layer::wing: return layers.frame;
            }
            return false;
        }

        auto layerLabel(mech::layer layer) -> const char* {
            switch (layer) {
                case mech::layer::plate: return "plate";
                case mech::layer::frame: return "frame";
                case mech::layer::inner: return "inner";
                case mech::layer::wing: return "wing";
            }
            return "?";
        }

        auto findActorByAlias(Reading context, const std::vector<Blueprints::Actor>& actors, renderer::Integer32 alias) -> base::maybe<Blueprints::Actor> {
            for (const auto& actor : actors) {
                if (not with<scene::actor::Identified>::exists(context, actor.id))
                    continue;
                if (with<scene::actor::Identified>::get(context, actor.id).scenicAlias == alias)
                    return actor;
            }
            return {};
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

        void eraseDescending(auto& vec, const std::set<std::size_t>& indices) {
            for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
                if (*it < vec.size())
                    vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(*it));
            }
        }

        template<typename Shape>
        auto shapeItem(const char* label, Shape current, Shape value) -> bool {
            const bool selected = current == value;
            if (ImGui::Selectable(label, selected))
                return not selected;
            if (selected)
                ImGui::SetItemDefaultFocus();
            return false;
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
        state.grid = with<scene::Interface>::createGrid(
            context,
            root,
            Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}),
            item<scene::Grid>{.geometry = *grid_geometry, .material = *grid_material, .opacity = 0.55f, .pattern_scale = pattern_scale});

        const Pos pivot{0.0f, 0.0f, 0.0f};
        const Pos camera_pos{24.0f, 20.0f, 40.0f};
        const auto camera = with<scene::Interface>::createCamera(
            context,
            root,
            Pose::from(camera_pos, HPB{36.87f, -29.74f, 0.0f}),
            60.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::CameraOrbit>::create(context, camera, pivot, glm::length(camera_pos - pivot));

        with<scene::Interface>::createLight(
            context,
            root,
            Pose::from(Pos{9.5f, 19.0f, 7.5f}, HPB{0.0f, 0.0f, 0.0f}),
            item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 60.0f});

        state.scene = root;
        state.camera = camera;
        state.loaded.clear();
        state.hovered.reset();
        state.selection.clear();
        state.layers = Layers{.plate = true, .frame = true, .inner = true};
        state.floorVisible.clear();
        state.floors.clear();
        state.levelOne.clear();
        state.levelTwo.clear();
        state.spaceMenu = {.target = {}, .close = false};

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
        state.hovered = asset_id;
        state.selection.clear();
        state.floorVisible.clear();
        syncVisuals(context);
    }

    void Blueprints::syncVisuals(Writing context) {
        for (const auto& actor : state.levelOne)
            destroyMeshActor(context, actor.id);
        for (const auto& actor : state.levelTwo)
            destroyMeshActor(context, actor.id);
        state.levelOne.clear();
        state.levelTwo.clear();
        state.floors.clear();

        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered)) {
            state.floorVisible.clear();
            return;
        }

        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data;
        const auto pack_one = *with<Assets>::find<meshpack::Asset>(context, Unit::Actions::name("Eltanin", "levelOne"));

        for (std::size_t i = 0; i < data.cells.size(); ++i) {
            const auto& cell = data.cells[i];
            if (auto frame = spawnFromPack(context, pack_one, mech::levelOne::mesh(cell.shape), cell.pose, mech::layer::frame, Source::cell, i, RGB{1.0f, 1.0f, 1.0f}, 1.0f))
                state.levelOne.push_back(*frame);
            if (auto inner = spawnFromPack(context, pack_one, mech::levelOne::innerMesh(cell.shape), cell.pose, mech::layer::inner, Source::cell, i, mech::slot::color(cell.role), cell.shape == mech::frame::shape::k8 ? 1.0f : 0.45f))
                state.levelOne.push_back(*inner);
        }
        for (std::size_t i = 0; i < data.stubs.size(); ++i) {
            const auto& stub = data.stubs[i];
            if (auto wing = spawnFromPack(context, pack_one, mech::levelOne::mesh(stub.shape), stub.pose, mech::layer::wing, Source::stub, i, RGB{1.0f, 1.0f, 1.0f}, 1.0f))
                state.levelOne.push_back(*wing);
        }
        for (std::size_t i = 0; i < data.hull.size(); ++i) {
            const auto& plate = data.hull[i];
            if (auto a = spawnFromPack(context, pack_one, mech::levelOne::mesh(plate.shape), plate.pose, mech::layer::plate, Source::plate, i, RGB{1.0f, 1.0f, 1.0f}, 1.0f))
                state.levelOne.push_back(*a);
        }

        for (const auto& actor : state.levelOne)
            state.floors[actor.floor].push_back(actor.id);
        for (const auto& actor : state.levelTwo)
            state.floors[actor.floor].push_back(actor.id);

        const auto previous = std::move(state.floorVisible);
        state.floorVisible.clear();
        for (const auto& [floor, _] : state.floors) {
            const auto it = previous.find(floor);
            state.floorVisible[floor] = it != previous.end() ? it->second : true;
        }

        applyLayers(context);
    }

    void Blueprints::deleteSelection(Writing context) {
        if (state.selection.empty())
            return;
        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            return;

        std::set<std::size_t> cells;
        std::set<std::size_t> stubs;
        std::set<std::size_t> plates;
        for (const auto alias : state.selection) {
            auto actor = findActorByAlias(context, state.levelOne, alias);
            if (not actor)
                actor = findActorByAlias(context, state.levelTwo, alias);
            if (not actor)
                continue;
            switch (actor->source) {
                case Source::cell: cells.insert(actor->index); break;
                case Source::stub: stubs.insert(actor->index); break;
                case Source::plate: plates.insert(actor->index); break;
            }
        }

        state.selection.clear();
        if (cells.empty() and stubs.empty() and plates.empty())
            return;

        auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
        eraseDescending(data->data.cells, cells);
        eraseDescending(data->data.stubs, stubs);
        eraseDescending(data->data.hull, plates);
        syncVisuals(context);
    }

    auto Blueprints::spawnFromPack(Writing context, meshpack::Asset::Id pack, const std::string& entry, const mech::Pose& pose, mech::layer layer, Source source, std::size_t index, RGB albedo, float opacity) -> base::maybe<Actor> {
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
        return Actor{.id = id, .layer = layer, .source = source, .index = index, .floor = static_cast<int>(pose.pos.y)};
    }

    void Blueprints::applyLayers(Writing context) {
        auto floorOn = [&](int floor) -> bool {
            const auto it = state.floorVisible.find(floor);
            return it == state.floorVisible.end() or it->second;
        };
        for (const auto& actor : state.levelOne)
            scene::actor::Mesh::Actions::setVisible(context, actor.id, layerVisible(state.layers, actor.layer) and floorOn(actor.floor));
        for (const auto& actor : state.levelTwo)
            scene::actor::Mesh::Actions::setVisible(context, actor.id, layerVisible(state.layers, actor.layer) and floorOn(actor.floor));
    }

    void Blueprints::draw(Writing context, bool& open) {
        if (not open)
            return;

        renderer::Integer32 under = renderer::Integer32{0};
        for (const auto [_, window] : context->aspect<system::Window>().items()) {
            under = window.current.under;
            break;
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) and not ImGui::GetIO().WantCaptureMouse and under != renderer::Integer32{0}) {
            const bool known = findActorByAlias(context, state.levelOne, under).exists() or findActorByAlias(context, state.levelTwo, under).exists();
            if (known and std::find(state.selection.begin(), state.selection.end(), under) == state.selection.end())
                state.selection.push_back(under);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete) and not ImGui::GetIO().WantCaptureKeyboard)
            deleteSelection(context);

        constexpr auto spaceMenuPopup = "##blueprints.spaceMenu";
        const bool spaceMenuOpen = ImGui::IsPopupOpen(spaceMenuPopup);
        if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
            if (spaceMenuOpen or state.spaceMenu.target.exists()) {
                state.spaceMenu.close = true;
            } else if (not ImGui::GetIO().WantCaptureKeyboard and under != renderer::Integer32{0}) {
                auto actor = findActorByAlias(context, state.levelOne, under);
                if (not actor)
                    actor = findActorByAlias(context, state.levelTwo, under);
                if (actor) {
                    state.spaceMenu = {.target = *actor, .close = false};
                    ImGui::OpenPopup(spaceMenuPopup);
                    ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
                }
            }
        }

        if (state.spaceMenu.target.exists()) {
            if (ImGui::BeginPopup(spaceMenuPopup)) {
                if (state.spaceMenu.close) {
                    ImGui::CloseCurrentPopup();
                    state.spaceMenu = {.target = {}, .close = false};
                } else {
                    const Actor target = *state.spaceMenu.target;
                    bool applied = false;
                    if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered)) {
                        ImGui::TextDisabled("No blueprint");
                    } else {
                        auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
                        if (target.source == Source::cell and target.index < data->data.cells.size()) {
                            auto& shape = data->data.cells[target.index].shape;
                            ImGui::TextUnformatted("Frame shape");
                            ImGui::Separator();
                            if (shapeItem("k8", shape, mech::frame::shape::k8)) { shape = mech::frame::shape::k8; applied = true; }
                            if (shapeItem("k7", shape, mech::frame::shape::k7)) { shape = mech::frame::shape::k7; applied = true; }
                            if (shapeItem("k6", shape, mech::frame::shape::k6)) { shape = mech::frame::shape::k6; applied = true; }
                            if (shapeItem("k4", shape, mech::frame::shape::k4)) { shape = mech::frame::shape::k4; applied = true; }
                        } else if (target.source == Source::stub and target.index < data->data.stubs.size()) {
                            auto& shape = data->data.stubs[target.index].shape;
                            ImGui::TextUnformatted("Wing shape");
                            ImGui::Separator();
                            if (shapeItem("w1111", shape, mech::wing::shape::w1111)) { shape = mech::wing::shape::w1111; applied = true; }
                            if (shapeItem("w121", shape, mech::wing::shape::w121)) { shape = mech::wing::shape::w121; applied = true; }
                            if (shapeItem("w2121", shape, mech::wing::shape::w2121)) { shape = mech::wing::shape::w2121; applied = true; }
                            if (shapeItem("w321", shape, mech::wing::shape::w321)) { shape = mech::wing::shape::w321; applied = true; }
                            if (shapeItem("w222", shape, mech::wing::shape::w222)) { shape = mech::wing::shape::w222; applied = true; }
                        } else if (target.source == Source::plate and target.index < data->data.hull.size()) {
                            auto& shape = data->data.hull[target.index].shape;
                            ImGui::TextUnformatted("Plate shape");
                            ImGui::Separator();
                            if (shapeItem("p1111", shape, mech::plate::shape::p1111)) { shape = mech::plate::shape::p1111; applied = true; }
                            if (shapeItem("p121", shape, mech::plate::shape::p121)) { shape = mech::plate::shape::p121; applied = true; }
                            if (shapeItem("p2121", shape, mech::plate::shape::p2121)) { shape = mech::plate::shape::p2121; applied = true; }
                            if (shapeItem("p222A", shape, mech::plate::shape::p222A)) { shape = mech::plate::shape::p222A; applied = true; }
                            if (shapeItem("p222V", shape, mech::plate::shape::p222V)) { shape = mech::plate::shape::p222V; applied = true; }
                        } else {
                            ImGui::TextDisabled("Stale target");
                        }
                    }
                    if (applied) {
                        ImGui::CloseCurrentPopup();
                        state.spaceMenu = {.target = {}, .close = false};
                        state.selection.clear();
                        syncVisuals(context);
                    }
                }
                ImGui::EndPopup();
            } else {
                state.spaceMenu = {.target = {}, .close = false};
            }
        }

        bool shown = open;
        ImVec2 blueprintsPos{};
        ImVec2 blueprintsSize{};
        if (ImGui::Begin("Blueprints", &shown)) {
            blueprintsPos = ImGui::GetWindowPos();
            blueprintsSize = ImGui::GetWindowSize();
            ImGui::TextDisabled("In-memory only — never writes .blueprint");
            ImGui::BeginChild("blueprintList", ImVec2{220.0f, 0.0f}, true);
            if (state.loaded.empty()) {
                ImGui::TextDisabled("No blueprints loaded.");
            } else {
                for (const auto asset_id : state.loaded) {
                    const auto& unit = with<Unit>::get(context, asset_id);
                    const auto& asset = with<::eltanin::resource::blueprint::Asset>::get(context, asset_id);
                    const bool hovered = state.hovered.exists() and *state.hovered == asset_id;
                    const char* label = asset.data.name.empty() ? unit.name.own.c_str() : asset.data.name.c_str();
                    if (ImGui::Selectable(label, hovered))
                        show(context, asset_id);
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("blueprintDetails", ImVec2{0.0f, 0.0f}, true);
            if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered)) {
                ImGui::TextDisabled("Select a blueprint.");
            } else {
                const auto& unit = with<Unit>::get(context, *state.hovered);
                const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data;
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
                if (under == renderer::Integer32{0}) {
                    ImGui::TextDisabled("—");
                } else {
                    const auto label = std::format("#{}", fqsm::internal::id::info_hash(static_cast<fqsm::internal::id::BaseType>(under)));
                    ImGui::TextUnformatted(label.c_str());
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Layers (levelOne)");
                bool visibilityChanged = false;
                visibilityChanged |= ImGui::Checkbox("plate", &state.layers.plate);
                visibilityChanged |= ImGui::Checkbox("frame", &state.layers.frame);
                visibilityChanged |= ImGui::Checkbox("inner", &state.layers.inner);
                ImGui::Separator();
                ImGui::TextUnformatted("Floors (y)");
                for (auto& [floor, visible] : state.floorVisible) {
                    const auto label = std::format("y = {}", floor);
                    ImGui::PushID(floor);
                    visibilityChanged |= ImGui::Checkbox(label.c_str(), &visible);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%zu)", state.floors[floor].size());
                    ImGui::PopID();
                }
                if (visibilityChanged)
                    applyLayers(context);
            }
            ImGui::EndChild();
        }
        ImGui::End();
        open = shown;

        if (not state.selection.empty()) {
            ImGui::SetNextWindowPos(ImVec2{blueprintsPos.x + blueprintsSize.x + 8.0f, blueprintsPos.y}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2{280.0f, 220.0f}, ImGuiCond_FirstUseEver);
            bool selectionOpen = true;
            if (ImGui::Begin("Selection", &selectionOpen)) {
                ImGui::TextDisabled("Del — remove from model (no disk write)");
                for (std::size_t index = 0; index < state.selection.size();) {
                    const auto alias = state.selection[index];
                    auto actor = findActorByAlias(context, state.levelOne, alias);
                    if (not actor)
                        actor = findActorByAlias(context, state.levelTwo, alias);
                    const auto hash = fqsm::internal::id::info_hash(static_cast<fqsm::internal::id::BaseType>(alias));
                    const auto row = actor ? std::format("{}  #{}", layerLabel(actor->layer), hash) : std::format("?  #{}", hash);
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TextUnformatted(row.c_str());
                    ImGui::SameLine();
                    const bool remove = ImGui::SmallButton("x");
                    ImGui::PopID();
                    if (remove)
                        state.selection.erase(state.selection.begin() + static_cast<std::ptrdiff_t>(index));
                    else
                        ++index;
                }
            }
            ImGui::End();
            if (not selectionOpen)
                state.selection.clear();
        }
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
