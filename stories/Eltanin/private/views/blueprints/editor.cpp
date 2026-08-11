#include "views/blueprints/editor.h"

#include "views/blueprints/geometry.h"
#include "views/blueprints/selection.h"

#include "mech/semantics/quarks.h"
#include "mech/semantics/shapes.h"
#include "mech/semantics/space.h"
#include "mech/semantics/subframe.h"
#include "mech/walls.h"

#include <eltanin/resources/assets.q1.h>
#include <eltanin/resources/blueprint.q1.h>
#include <eltanin/world.q1.h>
#include <fQSM/identifier.h>
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

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace eltanin::views {

    using namespace rmmr;
    using namespace rmmr::resource;

    namespace {

        constexpr float cursorOpacity = 0.18f;
        // Cell indices: inclusive pen. +49 (not +50) so the last cell stays inside grid lines −50…+50.
        constexpr integer cursorLatticeMin = -50;
        constexpr integer cursorLatticeMax = 49;
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

        auto cellWorldPos(const index3& lattice) -> Pos {
            const auto meters = mech::space::cell::center2local(mech::space::cell::index{lattice.x, lattice.y, lattice.z});
            return Pos{meters.x, meters.y, meters.z};
        }

        auto cellIndexFromMeters(float meters, float cell) -> integer {
            return static_cast<integer>(std::floor(meters / cell));
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
        state.cursorLattice = index3{.x = 0, .y = 0, .z = 0};
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

        state.scene = root;
        state.camera = camera;
        state.interframe = with<Assets>::find<meshpack::Asset>(context, Unit::Name::from("Eltanin", "interframe"));
        state.ghostMaterial = with<Assets>::find<::rmmr::resource::material::Asset>(context, Unit::Name::from("Eltanin", "clipboardGhost"));
        if (not state.ghostMaterial)
            return (void)context.refuse("eltanin::views::Blueprints::create: clipboardGhost material missing");
        state.quarkActors = {};
        state.clipboardActors = {};
        state.display = {.skeleton = true, .hull = true};
        state.walls = {.enabled = false, .cell = {}, .slots = {}, .face = 0};
        blueprints::selection::clear(state.selection);
        blueprints::selection::resetClipboard(state.selection);
        state.hovered.reset();
        state.spaceMenu = {.place = false, .close = false};
    }

    void Blueprints::show(Writing context, resource::blueprint::Asset::Id asset_id) {
        state.hovered = asset_id;
        blueprints::selection::clear(state.selection);
        syncVisuals(context);
        syncClipboardGhost(context);
        refreshWallCandidates(context);
    }

    void Blueprints::syncVisuals(Writing context) {
        if (not state.scene.exists() or not state.interframe.exists())
            return;
        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered)) {
            blueprints::geometry::clearActors(context, *state.scene, state.quarkActors);
            blueprints::geometry::clearActors(context, *state.scene, state.clipboardActors);
            blueprints::selection::clear(state.selection);
            return;
        }
        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data;
        blueprints::geometry::syncActors(context, *state.scene, *state.interframe, data, state.display, state.quarkActors);
        blueprints::selection::rematchAfterSync(context, state.selection, state.quarkActors);
    }

    void Blueprints::syncClipboardGhost(Writing context) {
        if (not state.scene.exists() or not state.interframe.exists() or not state.ghostMaterial.exists())
            return;
        if (blueprints::selection::clipboardEmpty(state.selection)) {
            blueprints::geometry::clearActors(context, *state.scene, state.clipboardActors);
            return;
        }
        constexpr float ghostOpacity = 0.45f;
        const RGB okTint{0.22f, 0.55f, 0.62f};
        const RGB blockedTint{0.62f, 0.22f, 0.18f};
        bool allowed = false;
        if (state.hovered.exists() and with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            allowed = blueprints::selection::canPaste(with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data, state.selection.clipboard);
        const auto tint = allowed ? okTint : blockedTint;
        // Prefer in-place pose/tint update — full destroy+create every frame trips fQSM (Mesh on a Node that is not new in the patch).
        if (blueprints::geometry::refreshGhostActors(context, *state.interframe, state.selection.clipboard, state.display, state.clipboardActors, tint, ghostOpacity))
            return;
        blueprints::geometry::syncGhostActors(context, *state.scene, *state.interframe, *state.ghostMaterial, state.selection.clipboard, state.display, state.clipboardActors, tint, ghostOpacity);
    }

    void Blueprints::refreshWallCandidates(Reading context) {
        if (not state.walls.enabled or not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered)) {
            state.walls.cell = {};
            state.walls.slots = {};
            state.walls.face = 0;
            return;
        }
        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data;
        base::maybe<std::size_t> cellIndex;
        for (std::size_t i = 0; i < data.cells.size(); ++i) {
            if (blueprints::selection::sameIndex3(data.cells[i].pose.pos, state.cursorLattice)) {
                cellIndex = i;
                break;
            }
        }
        // maybe<size_t>: never if(cellIndex)/if(not cellIndex) — integral T + operator T&() breaks empty/zero.
        if (not cellIndex.exists()) {
            state.walls.cell = {};
            state.walls.slots = {};
            state.walls.face = 0;
            return;
        }
        const bool cellChanged = not state.walls.cell.exists() or *state.walls.cell != *cellIndex;
        state.walls.cell = *cellIndex;
        state.walls.slots = mech::possibleWalls(data.cells[*cellIndex]);
        if (cellChanged)
            state.walls.face = 0;
        else if (state.walls.slots.empty())
            state.walls.face = 0;
        else if (state.walls.face >= state.walls.slots.size())
            state.walls.face = state.walls.slots.size() - 1;
    }

    auto Blueprints::handleWallMode(Writing context, renderer::Integer32 under) -> bool {
        if (not state.walls.enabled or ImGui::GetIO().WantCaptureMouse)
            return false;
        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            return false;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (not state.walls.cell.exists() or state.walls.slots.empty() or state.walls.face >= state.walls.slots.size())
                return false;
            const auto cell = *state.walls.cell;
            const auto wall = state.walls.slots[state.walls.face].wall;
            auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
            if (cell >= data->data.cells.size())
                return false;
            data->data.cells[cell].hull.walls.push_back(wall);
            persistHovered(context);
            syncVisuals(context);
            refreshWallCandidates(context);
            return true;
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (under == renderer::Integer32{0})
                return false;
            for (const auto& actor : state.quarkActors) {
                if (actor.kind != blueprints::geometry::QuarkActor::Kind::wall)
                    continue;
                if (not with<scene::actor::Identified>::exists(context, actor.id))
                    continue;
                if (with<scene::actor::Identified>::get(context, actor.id).scenicAlias != under)
                    continue;
                auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
                if (actor.cell >= data->data.cells.size())
                    return true;
                auto& walls = data->data.cells[actor.cell].hull.walls;
                if (actor.index < walls.size()) {
                    walls.erase(walls.begin() + static_cast<std::ptrdiff_t>(actor.index));
                    persistHovered(context);
                    syncVisuals(context);
                    refreshWallCandidates(context);
                }
                return true;
            }
            return false;
        }
        return false;
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
        auto lattice = index3{.x = state.cursorLattice.x, .y = state.currentFloor, .z = state.cursorLattice.z};

        if (state.camera.exists()) {
            if (const auto window = firstWindow(context); window and not ImGui::GetIO().WantCaptureMouse) {
                if (const auto viewport = firstViewport(context)) {
                    const auto mouse = with<system::Window>::get(context, *window).current.mouse;
                    if (const auto hit = rayHitFloor(context, *state.camera, *viewport, mouse, planeY)) {
                        lattice = index3{
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
        if (not open) {
            if (state.scene.exists())
                blueprints::geometry::clearActors(context, *state.scene, state.clipboardActors);
            return;
        }

        renderer::Integer32 under = renderer::Integer32{0};
        for (const auto [_, window] : context->aspect<system::Window>().items()) {
            under = window.current.under;
            break;
        }

        if (not ImGui::GetIO().WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
                ++state.currentFloor;
                syncGridToFloor(context);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
                --state.currentFloor;
                syncGridToFloor(context);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_B))
                blueprints::selection::toggleFocus(state.selection);
        }

        updateWorldCursor(context);
        refreshWallCandidates(context);

        if (state.walls.enabled and not state.walls.slots.empty() and not ImGui::GetIO().WantCaptureKeyboard) {
            const auto n = state.walls.slots.size();
            if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket))
                state.walls.face = (state.walls.face + n - 1) % n;
            if (ImGui::IsKeyPressed(ImGuiKey_RightBracket))
                state.walls.face = (state.walls.face + 1) % n;
        }

        if (not state.walls.enabled or not handleWallMode(context, under))
            blueprints::selection::handlePointer(context, state.selection, state.hovered, state.quarkActors, under);
        if (blueprints::selection::handleHotkeys(context, state.selection, state.hovered, state.quarkActors)) {
            persistHovered(context);
            syncVisuals(context);
            refreshWallCandidates(context);
        }
        blueprints::selection::handleClipboardHotkeys(state.selection);
        if (blueprints::selection::handleClipboardChords(context, state.selection, state.hovered, state.quarkActors)) {
            persistHovered(context);
            syncVisuals(context);
            refreshWallCandidates(context);
        }

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
                    const auto& dataView = with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data;
                    const bool occupied = blueprints::selection::cellOccupied(dataView, state.cursorLattice);
                    if (occupied) {
                        if (ImGui::Selectable("erase")) {
                            {
                                auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
                                data->data.cells.erase(std::remove_if(data->data.cells.begin(), data->data.cells.end(), [&](const mech::Blueprint::Cell& cell) { return blueprints::selection::sameIndex3(cell.pose.pos, state.cursorLattice); }), data->data.cells.end());
                            }
                            ImGui::CloseCurrentPopup();
                            state.spaceMenu = {.place = false, .close = false};
                            blueprints::selection::clear(state.selection);
                            persistHovered(context);
                            syncVisuals(context);
                        }
                    } else {
                        bool applied = false;
                        auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
                        applied = frameShapePicks([&](mech::frame::shape shape) {
                            const auto pose = mech::space::cell::Pose{.pos = state.cursorLattice, .ori = 0};
                            data->data.cells.push_back(mech::Blueprint::Cell{
                                .pose = pose,
                                .shape = shape,
                                .frame = {.knots = mech::quarks::seedCorners(shape), .halfChords = mech::quarks::seedHalfChords(shape)},
                                .hull = {.walls = {}},
                            });
                        });
                        if (applied) {
                            ImGui::CloseCurrentPopup();
                            state.spaceMenu = {.place = false, .close = false};
                            blueprints::selection::clear(state.selection);
                            persistHovered(context);
                            syncVisuals(context);
                        }
                    }
                }
                ImGui::EndPopup();
            } else {
                state.spaceMenu = {.place = false, .close = false};
            }
        }

        bool shown = open;
        ImVec2 blueprintsPos{};
        ImVec2 blueprintsSize{};
        if (ImGui::Begin("Blueprints", &shown)) {
            blueprintsPos = ImGui::GetWindowPos();
            blueprintsSize = ImGui::GetWindowSize();
            ImGui::BeginChild("blueprintList", ImVec2{220.0f, 0.0f}, true);
            const auto pickBlueprint = [&](resource::blueprint::Asset::Id assetId) {
                const auto& unit = with<Unit>::get(context, assetId);
                const auto& asset = with<::eltanin::resource::blueprint::Asset>::get(context, assetId);
                const bool hovered = state.hovered.exists() and *state.hovered == assetId;
                const char* label = asset.data.name.empty() ? unit.name.own.c_str() : asset.data.name.c_str();
                ImGui::PushID(reinterpret_cast<const void*>(assetId.raw()));
                if (ImGui::Selectable(label, hovered))
                    show(context, assetId);
                ImGui::PopID();
            };

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##newBlueprint", "New name...", catalog.newName.data(), catalog.newName.size());
            if (ImGui::Button("Create ship", ImVec2{-1.0f, 0.0f})) {
                if (const auto id = catalog.createNew(context, BlueprintShelf::ships, catalog.newName.data())) {
                    catalog.newName = {};
                    show(context, *id);
                }
            }
            if (ImGui::Button("Create prefab", ImVec2{-1.0f, 0.0f})) {
                if (const auto id = catalog.createNew(context, BlueprintShelf::prefabs, catalog.newName.data())) {
                    catalog.newName = {};
                    show(context, *id);
                }
            }

            ImGui::Separator();
            ImGui::TextDisabled("Editor");
            if (catalog.unnamed and with<::eltanin::resource::blueprint::Asset>::exists(context, *catalog.unnamed))
                pickBlueprint(*catalog.unnamed);
            else
                ImGui::TextDisabled("_unnamed missing");

            if (ImGui::CollapsingHeader("Ships")) {
                if (catalog.ships.empty()) {
                    ImGui::TextDisabled("No ships.");
                } else {
                    for (const auto assetId : catalog.ships)
                        pickBlueprint(assetId);
                }
            }

            if (ImGui::CollapsingHeader("Prefabs")) {
                if (catalog.prefabs.empty()) {
                    ImGui::TextDisabled("No prefabs.");
                } else {
                    for (const auto assetId : catalog.prefabs)
                        pickBlueprint(assetId);
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
                bool displayChanged = false;
                displayChanged |= ImGui::Checkbox("Skeleton", &state.display.skeleton);
                ImGui::SameLine();
                displayChanged |= ImGui::Checkbox("Hull", &state.display.hull);
                if (displayChanged) {
                    blueprints::selection::clear(state.selection);
                    syncVisuals(context);
                    syncClipboardGhost(context);
                }
                if (ImGui::Checkbox("Wall place/remove", &state.walls.enabled)) {
                    if (state.walls.enabled and not state.display.hull) {
                        state.display.hull = true;
                        syncVisuals(context);
                    }
                    refreshWallCandidates(context);
                }
                if (state.walls.enabled) {
                    ImGui::TextDisabled("[ ] cycle face · LMB place · RMB remove wall · select + Del");
                    if (not state.walls.cell.exists()) {
                        ImGui::TextDisabled("Wall: no cell under cursor");
                    } else if (state.walls.slots.empty()) {
                        ImGui::TextDisabled("Wall: no free faces");
                    } else {
                        const auto& slot = state.walls.slots[state.walls.face];
                        const auto codeIt = mech::subframe::membrane::specs.find(slot.wall.kind);
                        const auto code = codeIt != mech::subframe::membrane::specs.end() ? codeIt->second.code : "?";
                        ImGui::Text("Wall: face %zu/%zu · %.*s · ori %d", state.walls.face + 1, state.walls.slots.size(), static_cast<int>(code.size()), code.data(), static_cast<int>(slot.wall.ori));
                    }
                }
                ImGui::Separator();
                std::size_t knots = 0;
                std::size_t halfChords = 0;
                std::size_t walls = 0;
                for (const auto& cell : data.cells) {
                    knots += cell.frame.knots.size();
                    halfChords += cell.frame.halfChords.size();
                    walls += cell.hull.walls.size();
                }
                ImGui::Text("Cells: %zu", data.cells.size());
                ImGui::Text("Knots: %zu", knots);
                ImGui::Text("Half-chords: %zu", halfChords);
                ImGui::Text("Walls: %zu", walls);
                ImGui::Text("Actors: %zu", state.quarkActors.size());
                ImGui::Text("Selection: %zu", state.selection.aliases.size());
                ImGui::Text("Cursor [%d, %d, %d]", state.cursorLattice.x, state.cursorLattice.y, state.cursorLattice.z);
                ImGui::Text("Floor: %d", state.currentFloor);
                ImGui::TextDisabled("MMB orbit · PgUp/PgDn · Space · LMB/RMB±Shift · Del · WASD/QE · Shift rotate · Ctrl+C/V · B");
                if (under == renderer::Integer32{0}) {
                    ImGui::TextDisabled("Under: —");
                } else {
                    ImGui::Text("Under: #%s", fqsm::internal::id::info_hash(static_cast<fqsm::internal::id::BaseType>(under)).c_str());
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
        open = shown;

        if (blueprints::selection::drawPanel(context, state.selection, blueprintsPos, blueprintsSize, state.hovered, state.quarkActors)) {
            persistHovered(context);
            syncVisuals(context);
            refreshWallCandidates(context);
        }
        if (blueprints::selection::drawClipboardPanel(context, state.selection, blueprintsPos, blueprintsSize, state.hovered)) {
            persistHovered(context);
            syncVisuals(context);
            refreshWallCandidates(context);
        }
        syncClipboardGhost(context);
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
