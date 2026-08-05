#include "views/blueprints.h"

#include "mech/semantics/space.h"

#include <eltanin/resources/assets.q1.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/root.q1.h>

#include <imgui.h>

#include <filesystem>
#include <numbers>

namespace eltanin::views {

    using namespace rmmr;
    using namespace rmmr::resource;

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
                Locator{.pos = Pos{0.0f, static_cast<float>(levels[index]) * cell, 0.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
                item<scene::Grid>{.geometry = *grid_geometry, .material = *grid_material, .opacity = 0.35f, .pattern_scale = pattern_scale});
        }

        const Pos camera_pos{12.0f, 10.0f, 20.0f};
        const auto camera = with<scene::Interface>::createCamera(
            context,
            root,
            Locator{.pos = camera_pos, .euler = HPB{36.87f, -29.74f, 0.0f}},
            100.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::Camera3d>::create(context, camera);

        with<scene::Interface>::createLight(
            context,
            root,
            Locator{.pos = Pos{9.5f, 19.0f, 7.5f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 60.0f});

        state.scene = root;
        state.camera = camera;
        state.loaded.clear();
        state.selected.reset();

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
            state.selected = state.loaded.front();
    }

    void Blueprints::draw(Reading context, bool& open) {
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
                        state.selected = asset_id;
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
