#include "views/blueprints/editor.h"

#include "mech/semantics/quarks.h"
#include "mech/semantics/shapes.h"
#include "mech/semantics/space.h"

#include <eltanin/resources/assets.q1.h>
#include <eltanin/resources/blueprint.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/controller/cameraOrbit.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/window.q1.h>

#include <base/logging.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace eltanin::views {

    using namespace rmmr;
    using namespace rmmr::resource;

    namespace {

        constexpr float cursorOpacity = 0.18f;
        // Cell indices: inclusive pen. +49 (not +50) so the last cell stays inside grid lines −50…+50.
        constexpr base::common_types::integer cursorLatticeMin = -50;
        constexpr base::common_types::integer cursorLatticeMax = 49;
        constexpr float gridOpacity = 0.88f;

        void applyOrbitPose(Writing context, scene::Camera::Id camera) {
            if (not with<controller::CameraOrbit>::exists(context, camera))
                return;
            const auto& orbit = with<controller::CameraOrbit>::get(context, camera);
            HPB hpb = orbit.hpb;
            hpb.z = 0.0f;
            const quat rotation = Pose::from(Pos{0.0f, 0.0f, 0.0f}, hpb).rotation;
            const vec3 forward = glm::normalize(rotation * vec3{0.0f, 0.0f, -1.0f});
            auto node = with<scene::Node>::modify(context, camera);
            node->pose.rotation = rotation;
            node->pose.position = orbit.pivot - forward * orbit.distance;
        }

        auto cellWorldPos(const base::common_types::index3& lattice) -> Pos {
            const auto meters = mech::space::cell::center2local(mech::space::cell::index{lattice.x, lattice.y, lattice.z});
            return Pos{meters.x, meters.y, meters.z};
        }

        auto cellIndexFromMeters(float meters, float cell) -> base::common_types::integer {
            return static_cast<base::common_types::integer>(std::floor(meters / cell));
        }

        auto firstWindow(Reading context) -> base::maybe<system::Window::Id> {
            for (const auto [id, _] : context->aspect<system::Window>().items())
                return id;
            return {};
        }

        auto firstViewport(Reading context) -> base::maybe<system::Viewport::Id> {
            for (const auto [id, _] : context->aspect<system::Viewport>().items())
                return id;
            return {};
        }

        auto rayHitFloor(Reading context, scene::Camera::Id camera, system::Viewport::Id viewport, index2 mouse, float planeY) -> base::maybe<Pos> {
            const auto& vp = with<system::Viewport>::get(context, viewport);
            if (vp.size.x <= 0 or vp.size.y <= 0)
                return {};
            const float ndcX = (2.0f * static_cast<float>(mouse.x - vp.origin.x) / static_cast<float>(vp.size.x)) - 1.0f;
            const float ndcY = 1.0f - (2.0f * static_cast<float>(mouse.y - vp.origin.y) / static_cast<float>(vp.size.y));
            const float aspect = static_cast<float>(vp.size.x) / static_cast<float>(vp.size.y);
            const mat4 inv = glm::inverse(with<scene::Camera>::view_projection(context, camera, aspect));
            const vec4 nearH = inv * vec4{ndcX, ndcY, -1.0f, 1.0f};
            const vec4 farH = inv * vec4{ndcX, ndcY, 1.0f, 1.0f};
            if (std::abs(nearH.w) < 1e-8f or std::abs(farH.w) < 1e-8f)
                return {};
            const Pos origin{nearH.x / nearH.w, nearH.y / nearH.w, nearH.z / nearH.w};
            const Pos farP{farH.x / farH.w, farH.y / farH.w, farH.z / farH.w};
            const vec3 dir = farP - origin;
            if (std::abs(dir.y) < 1e-8f)
                return {};
            const float t = (planeY - origin.y) / dir.y;
            if (t < 0.0f)
                return {};
            return origin + dir * t;
        }

        auto sameIndex3(const base::common_types::index3& a, const base::common_types::index3& b) -> bool {
            return a.x == b.x and a.y == b.y and a.z == b.z;
        }

        auto sameGridPose(const mech::space::grid::Pose& a, const mech::space::grid::Pose& b) -> bool {
            return sameIndex3(a.pos, b.pos) and a.ori == b.ori;
        }

        auto frameShapePicks(auto&& onPick) -> bool {
            bool applied = false;
            ImGui::TextUnformatted("Spawn frame");
            ImGui::Separator();
            if (ImGui::Selectable("k8")) { onPick(mech::frame::shape::k8); applied = true; }
            if (ImGui::Selectable("k7")) { onPick(mech::frame::shape::k7); applied = true; }
            if (ImGui::Selectable("k6")) { onPick(mech::frame::shape::k6); applied = true; }
            if (ImGui::Selectable("k4")) { onPick(mech::frame::shape::k4); applied = true; }
            ImGui::Separator();
            if (ImGui::Selectable("k4f1111")) { onPick(mech::frame::shape::k4f1111); applied = true; }
            if (ImGui::Selectable("k3f121")) { onPick(mech::frame::shape::k3f121); applied = true; }
            if (ImGui::Selectable("k4f2121")) { onPick(mech::frame::shape::k4f2121); applied = true; }
            if (ImGui::Selectable("k3f222")) { onPick(mech::frame::shape::k3f222); applied = true; }
            return applied;
        }

    } // namespace

    void Blueprints::create(Writing context) {
        const auto grid_geometry = with<Assets>::find<geometry::Asset>(context, Unit::Name::from("rmmr", "grid"));
        const auto grid_material = with<Assets>::find<rmmr::resource::material::Asset>(context, Unit::Name::from("rmmr", "grid"));
        if (not grid_geometry or not grid_material)
            return (void)context.refuse("eltanin::views::Blueprints::create: grid assets missing");

        const auto kube_geometry = with<Assets>::find<geometry::Asset>(context, Unit::Name::from("rmmr", "kube"));
        const auto cursor_material = with<Assets>::find<rmmr::resource::material::Asset>(context, Unit::Name::from("Eltanin", "type"));
        if (not kube_geometry or not cursor_material)
            return (void)context.refuse("eltanin::views::Blueprints::create: kube / lit-transparent material missing");

        const auto device = with<World>::get_global(context).window;
        if (not device)
            return (void)context.refuse("eltanin::views::Blueprints::create: World window missing");

        const auto root = with<scene::Interface>::createScene(context);

        const float cell = mech::space::local::edge2meters;
        const float patternScale = 1.0f / cell;
        state.currentFloor = 0;
        state.cursorLattice = base::common_types::index3{.x = 0, .y = 0, .z = 0};
        state.grid = with<scene::Interface>::createGrid(context, root, *device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Grid>{.geometry = *grid_geometry, .material = *grid_material, .opacity = gridOpacity, .patternScale = patternScale});

        const float edge = mech::space::local::edge2meters;
        const auto cursorResolved = meshpack::Asset::Resolved{
            .geometry = *kube_geometry,
            .entry = geometry::EntryId{0},
            .surfaces = {{geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = *cursor_material, .textures = {}}}},
            .texpack = {},
        };
        state.worldCursor = with<scene::Interface>::createMeshActor(context, root, Pose::from(cellWorldPos(state.cursorLattice), HPB{0.0f, 0.0f, 0.0f}), cursorResolved, with<scene::actor::MeshState>::defaults(RGB{0.35f, 0.95f, 1.0f}, cursorOpacity, vec3{edge, edge, edge}));

        const Pos pivot{0.0f, 0.0f, 0.0f};
        const Pos camera_pos{24.0f, 20.0f, 40.0f};
        const auto camera = with<scene::Interface>::createCamera(context, root, Pose::from(camera_pos, HPB{36.87f, -29.74f, 0.0f}), 60.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::CameraOrbit>::create(context, camera, pivot, glm::length(camera_pos - pivot));
        applyOrbitPose(context, camera);

        with<scene::Interface>::createLight(context, root, Pose::from(Pos{9.5f, 19.0f, 7.5f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 120.0f});

        // TEMP: dump interframe entries on a 4×0×4 m lattice to inspect origin bake.
        if (const auto interframe = with<Assets>::find<meshpack::Asset>(context, Unit::Name::from("Eltanin", "interframe"))) {
            const auto& pack = with<meshpack::Asset>::get(context, *interframe);
            vector<string> names;
            names.reserve(pack.entries.size());
            for (const auto& [name, _] : pack.entries)
                names.push_back(name);
            std::ranges::sort(names);
            const auto cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(names.size()))));
            for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                const auto resolved = with<meshpack::Asset>::resolve(context, *interframe, names[static_cast<std::size_t>(i)]);
                if (not resolved)
                    continue;
                const auto& geometry = with<geometry::Asset>::get(context, resolved->geometry);
                const auto& origin = geometry.entries[resolved->entry].origin;
                const int col = cols > 0 ? i % cols : 0;
                const int row = cols > 0 ? i / cols : 0;
                const Pos at{static_cast<float>(col) * 4.0f, 0.0f, static_cast<float>(row) * 4.0f};
                base::message("eltanin TEMP interframe '{}' origin=[{:.3f},{:.3f},{:.3f}] @ [{:.0f},{:.0f},{:.0f}]", names[static_cast<std::size_t>(i)], origin.x, origin.y, origin.z, at.x, at.y, at.z);
                (void)with<scene::Interface>::createMeshActor(context, root, Pose::from(at, HPB{0.0f, 0.0f, 0.0f}), *resolved);
            }
        }

        state.scene = root;
        state.camera = camera;
        state.hovered.reset();
        state.spaceMenu = {.place = false, .close = false};
    }

    void Blueprints::show(Writing, resource::blueprint::Asset::Id asset_id) {
        state.hovered = asset_id;
    }

    void Blueprints::syncGridToFloor(Writing context) {
        if (not state.grid.exists() or not with<scene::Node>::exists(context, *state.grid))
            return;
        const float cell = mech::space::local::edge2meters;
        const float y = static_cast<float>(state.currentFloor) * cell;
        with<scene::Node>::modify(context, *state.grid)->pose = Pose::from(Pos{0.0f, y, 0.0f}, HPB{0.0f, 0.0f, 0.0f});
        if (state.camera.exists() and with<controller::CameraOrbit>::exists(context, *state.camera)) {
            auto orbit = with<controller::CameraOrbit>::modify(context, *state.camera);
            orbit->pivot.y = static_cast<float>(state.currentFloor) * cell;
            applyOrbitPose(context, *state.camera);
        }
    }

    void Blueprints::updateWorldCursor(Writing context) {
        if (not state.worldCursor.exists() or not with<scene::Node>::exists(context, *state.worldCursor))
            return;

        const float cell = mech::space::local::edge2meters;
        const float planeY = static_cast<float>(state.currentFloor) * cell;
        auto lattice = base::common_types::index3{.x = state.cursorLattice.x, .y = state.currentFloor, .z = state.cursorLattice.z};

        if (state.camera.exists()) {
            if (const auto window = firstWindow(context); window and not ImGui::GetIO().WantCaptureMouse) {
                if (const auto viewport = firstViewport(context)) {
                    const auto mouse = with<system::Window>::get(context, *window).current.mouse;
                    if (const auto hit = rayHitFloor(context, *state.camera, *viewport, mouse, planeY)) {
                        lattice = base::common_types::index3{
                            .x = cellIndexFromMeters(hit->x, cell),
                            .y = state.currentFloor,
                            .z = cellIndexFromMeters(hit->z, cell),
                        };
                    }
                }
            }
        }

        lattice.y = state.currentFloor;
        lattice.x = std::clamp(lattice.x, cursorLatticeMin, cursorLatticeMax);
        lattice.z = std::clamp(lattice.z, cursorLatticeMin, cursorLatticeMax);

        state.cursorLattice = lattice;
        with<scene::Node>::modify(context, *state.worldCursor)->pose = Pose::from(cellWorldPos(state.cursorLattice), HPB{0.0f, 0.0f, 0.0f});
    }

    void Blueprints::persistHovered(Writing context) {
        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            return;
        if (not with<::eltanin::resource::blueprint::Loader>::exists(context, *state.hovered))
            return;
        ::eltanin::resource::blueprint::Loader::Actions::save(context, *state.hovered);
    }

    void Blueprints::draw(Writing context, bool& open, BlueprintCatalog& catalog) {
        if (not open)
            return;

        if (not ImGui::GetIO().WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
                ++state.currentFloor;
                syncGridToFloor(context);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
                --state.currentFloor;
                syncGridToFloor(context);
            }
        }

        updateWorldCursor(context);

        constexpr auto spaceMenuPopup = "##blueprints.spaceMenu";
        const bool spaceMenuOpen = ImGui::IsPopupOpen(spaceMenuPopup);
        const bool spaceMenuActive = state.spaceMenu.place;
        if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
            if (spaceMenuOpen or spaceMenuActive) {
                state.spaceMenu.close = true;
            } else if (not ImGui::GetIO().WantCaptureKeyboard) {
                state.spaceMenu = {.place = true, .close = false};
                ImGui::OpenPopup(spaceMenuPopup);
                ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
            }
        }

        if (spaceMenuActive) {
            if (ImGui::BeginPopup(spaceMenuPopup)) {
                if (state.spaceMenu.close) {
                    ImGui::CloseCurrentPopup();
                    state.spaceMenu = {.place = false, .close = false};
                } else if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered)) {
                    ImGui::TextDisabled("No blueprint");
                } else {
                    bool applied = false;
                    auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
                    applied = frameShapePicks([&](mech::frame::shape shape) {
                        const auto cell = mech::space::cell::Pose{
                            .pos = state.cursorLattice,
                            .ori = 0,
                        };
                        for (const auto& knot : mech::quarks::seedCorners(shape, cell)) {
                            bool occupied = false;
                            for (const auto& existing : data->data.knots) {
                                if (sameIndex3(existing.pose.pos, knot.pose.pos)) {
                                    occupied = true;
                                    break;
                                }
                            }
                            if (not occupied)
                                data->data.knots.push_back(knot);
                        }
                        for (const auto& chord : mech::quarks::seedChords(shape, cell)) {
                            bool occupied = false;
                            for (const auto& existing : data->data.chords) {
                                if (sameGridPose(existing.pose, chord.pose)) {
                                    occupied = true;
                                    break;
                                }
                            }
                            if (not occupied)
                                data->data.chords.push_back(chord);
                        }
                    });
                    if (applied) {
                        ImGui::CloseCurrentPopup();
                        state.spaceMenu = {.place = false, .close = false};
                        persistHovered(context);
                    }
                }
                ImGui::EndPopup();
            } else {
                state.spaceMenu = {.place = false, .close = false};
            }
        }

        bool shown = open;
        if (ImGui::Begin("Blueprints", &shown)) {
            ImGui::BeginChild("blueprintList", ImVec2{220.0f, 0.0f}, true);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##newBlueprint", "New name...", catalog.newName.data(), catalog.newName.size());
            if (ImGui::Button("Create new", ImVec2{-1.0f, 0.0f})) {
                if (const auto id = catalog.createNew(context, catalog.newName.data())) {
                    catalog.newName = {};
                    show(context, *id);
                }
            }
            ImGui::Separator();
            if (catalog.items.empty()) {
                ImGui::TextDisabled("No blueprints loaded.");
            } else {
                for (const auto asset_id : catalog.items) {
                    const auto& unit = with<Unit>::get(context, asset_id);
                    const auto& asset = with<::eltanin::resource::blueprint::Asset>::get(context, asset_id);
                    const bool hovered = state.hovered.exists() and *state.hovered == asset_id;
                    const char* label = asset.data.name.empty() ? unit.name.own.c_str() : asset.data.name.c_str();
                    ImGui::PushID(reinterpret_cast<const void*>(asset_id.raw()));
                    if (ImGui::Selectable(label, hovered))
                        show(context, asset_id);
                    ImGui::PopID();
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
                ImGui::Text("File: %s", unit.name.own.c_str());
                ImGui::Text("Name: %s", data.name.c_str());
                ImGui::Text("Manufacturer: %s", data.author.c_str());
                ImGui::Separator();
                ImGui::Text("Knots: %zu", data.knots.size());
                ImGui::Text("Chords: %zu", data.chords.size());
                ImGui::Text("Cursor [%d, %d, %d]", state.cursorLattice.x, state.cursorLattice.y, state.cursorLattice.z);
                ImGui::Text("Floor: %d", state.currentFloor);
                ImGui::TextDisabled("PgUp / PgDn · Space — seed k* into blueprint (no mesh actors)");
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
