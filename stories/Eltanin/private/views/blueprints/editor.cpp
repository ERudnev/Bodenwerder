#include "views/blueprints/editor.h"

#include "views/blueprints/geometry.h"
#include "views/blueprints/history.h"
#include "views/blueprints/selection.h"

#include "mech/semantics/quarks.h"
#include "mech/semantics/shapes.h"
#include "mech/semantics/space.h"
#include "mech/semantics/subframe.h"
#include "views/blueprints/membraneSlots.h"
#include "views/blueprints/mountPlacement.h"

#include <eltanin/resources/assets.q1.h>
#include <eltanin/mech/blueprint.q1.h>
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
#include <limits>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace eltanin::views {

    using namespace rmmr;
    using namespace rmmr::resource;
    using blueprints::Cell;

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

        struct MouseRay {
            Pos origin;
            vec3 dir;
        };

        auto mouseRay(Reading context, scene::Camera::Id camera, system::Viewport::Id viewport, index2 mouse) -> base::maybe<MouseRay> {
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
            if (glm::length(dir) < 1e-8f)
                return {};
            return MouseRay{.origin = origin, .dir = dir};
        }

        auto rayHitFloor(Reading context, scene::Camera::Id camera, system::Viewport::Id viewport, index2 mouse, float planeY) -> base::maybe<Pos> {
            const auto ray = mouseRay(context, camera, viewport, mouse);
            if (not ray.has_value() or std::abs(ray->dir.y) < 1e-8f)
                return {};
            const float t = (planeY - ray->origin.y) / ray->dir.y;
            if (t < 0.0f)
                return {};
            return ray->origin + ray->dir * t;
        }

        auto cornerWorld(const mech::space::cell::Placement& pose, mech::cube::Corner corner) -> Pos {
            const auto center = mech::space::cell::center2local(mech::space::cell::index{pose.cell.x, pose.cell.y, pose.cell.z});
            const auto local = mech::space::orient::cell2local(static_cast<mech::space::orient::key>(pose.ori), mech::cube::corners[static_cast<std::size_t>(corner)]);
            return Pos{center.x + local.x, center.y + local.y, center.z + local.z};
        }

        auto faceWorldLoop(const Cell& cell, mech::frame::FaceIndex face) -> std::vector<Pos> {
            const auto shapeIndex = static_cast<std::size_t>(cell.shape);
            const auto faceIndex = static_cast<std::size_t>(face);
            if (shapeIndex >= mech::frame::faces.size() or faceIndex >= mech::frame::faces[shapeIndex].size())
                return {};
            const auto& loop = mech::frame::faces[shapeIndex][faceIndex];
            std::vector<Pos> out;
            out.reserve(loop.size());
            for (const auto corner : loop)
                out.push_back(cornerWorld(cell.placement, corner));
            return out;
        }

        // Möller–Trumbore; t along ray.dir (not required unit).
        auto rayHitTriangle(const Pos& origin, const vec3& dir, const Pos& a, const Pos& b, const Pos& c, float& tOut) -> bool {
            constexpr float eps = 1e-6f;
            const vec3 ab = b - a;
            const vec3 ac = c - a;
            const vec3 pvec = glm::cross(dir, ac);
            const float det = glm::dot(ab, pvec);
            if (std::abs(det) < eps)
                return false;
            const float invDet = 1.0f / det;
            const vec3 tvec = origin - a;
            const float u = glm::dot(tvec, pvec) * invDet;
            if (u < 0.0f or u > 1.0f)
                return false;
            const vec3 qvec = glm::cross(tvec, ab);
            const float v = glm::dot(dir, qvec) * invDet;
            if (v < 0.0f or u + v > 1.0f)
                return false;
            const float t = glm::dot(ac, qvec) * invDet;
            if (t < eps)
                return false;
            tOut = t;
            return true;
        }

        auto rayHitPolygon(const Pos& origin, const vec3& dir, const std::vector<Pos>& loop, float& tOut) -> bool {
            if (loop.size() < 3)
                return false;
            bool hit = false;
            float best = tOut;
            for (std::size_t i = 1; i + 1 < loop.size(); ++i) {
                float t = 0.0f;
                if (rayHitTriangle(origin, dir, loop[0], loop[i], loop[i + 1], t) and (not hit or t < best)) {
                    best = t;
                    hit = true;
                }
            }
            if (hit)
                tOut = best;
            return hit;
        }

        auto projectWorld(Reading context, scene::Camera::Id camera, system::Viewport::Id viewport, const Pos& world) -> base::maybe<ImVec2> {
            const auto& vp = with<system::Viewport>::get(context, viewport);
            if (vp.size.x <= 0 or vp.size.y <= 0)
                return {};
            const float aspect = static_cast<float>(vp.size.x) / static_cast<float>(vp.size.y);
            const vec4 clip = with<scene::Camera>::view_projection(context, camera, aspect) * vec4{world.x, world.y, world.z, 1.0f};
            if (std::abs(clip.w) < 1e-8f)
                return {};
            const float ndcX = clip.x / clip.w;
            const float ndcY = clip.y / clip.w;
            const float ndcZ = clip.z / clip.w;
            if (ndcZ < -1.0f or ndcZ > 1.0f)
                return {};
            return ImVec2{
                static_cast<float>(vp.origin.x) + (ndcX + 1.0f) * 0.5f * static_cast<float>(vp.size.x),
                static_cast<float>(vp.origin.y) + (1.0f - ndcY) * 0.5f * static_cast<float>(vp.size.y),
            };
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

        constexpr float placePreviewOpacity = 0.45f;
        constexpr RGB placePreviewTint{0.22f, 0.55f, 0.62f};

        void dropSpacePreview(Writing context, Blueprints::State& state) {
            if (state.spaceMenu.preview.has_value() and state.mainScene.root.has_value())
                blueprints::geometry::destroyMeshActor(context, *state.mainScene.root, *state.spaceMenu.preview);
            state.spaceMenu.preview.reset();
            state.spaceMenu.previewMount.reset();
            state.spaceMenu.previewTransform.reset();
        }

        void closeSpaceMenu(Writing context, Blueprints::State& state) {
            dropSpacePreview(context, state);
            state.spaceMenu.place = false;
            state.spaceMenu.close = false;
        }

        void ensurePlacePreview(Writing context, Blueprints::State& state, mech::Mount::Id id, const mech::Attachment& attachment, const blueprints::mountEditor::Fits& fits) {
            if (state.spaceMenu.previewMount.has_value() and *state.spaceMenu.previewMount == id) {
                if (state.spaceMenu.preview.has_value() or not state.spaceMenu.previewTransform.has_value())
                    return;
                if (not state.mainScene.root.has_value() or not state.ghostMaterial.has_value())
                    return;
                state.spaceMenu.preview = blueprints::geometry::spawnGhostMount(context, *state.mainScene.root, *state.ghostMaterial, id, *state.spaceMenu.previewTransform, placePreviewTint, placePreviewOpacity);
                return;
            }
            dropSpacePreview(context, state);
            const auto seating = blueprints::mountEditor::seatingOn(attachment, fits, state.mounts.points);
            if (not seating)
                return;
            state.spaceMenu.previewMount = id;
            state.spaceMenu.previewTransform = *seating;
            if (not state.mainScene.root.has_value() or not state.ghostMaterial.has_value())
                return;
            state.spaceMenu.preview = blueprints::geometry::spawnGhostMount(context, *state.mainScene.root, *state.ghostMaterial, id, *seating, placePreviewTint, placePreviewOpacity);
        }

        void applyPlacePreviewOri(Writing context, Blueprints::State& state) {
            if (not state.spaceMenu.previewMount.has_value() or not state.spaceMenu.previewTransform.has_value())
                return;
            if (not ImGui::GetIO().KeyShift or ImGui::GetIO().KeyCtrl)
                return;
            using Semiaxis = mech::space::orient::Semiaxis;
            base::maybe<Semiaxis> axis;
            if (ImGui::IsKeyPressed(ImGuiKey_A)) axis = Semiaxis::Yn;
            else if (ImGui::IsKeyPressed(ImGuiKey_D)) axis = Semiaxis::Yp;
            else if (ImGui::IsKeyPressed(ImGuiKey_W)) axis = Semiaxis::Xp;
            else if (ImGui::IsKeyPressed(ImGuiKey_S)) axis = Semiaxis::Xn;
            else if (ImGui::IsKeyPressed(ImGuiKey_Q)) axis = Semiaxis::Zn;
            else if (ImGui::IsKeyPressed(ImGuiKey_E)) axis = Semiaxis::Zp;
            if (not axis)
                return;
            const auto found = state.mountSpins.find(*state.spaceMenu.previewMount);
            if (found == state.mountSpins.end())
                return;
            const auto next = blueprints::mountEditor::applyOri(*state.spaceMenu.previewTransform, found->second, mech::space::orient::turn(*axis)[0]);
            if (not next)
                return;
            state.spaceMenu.previewTransform = *next;
            if (state.spaceMenu.preview.has_value())
                blueprints::geometry::poseGhostMount(context, *state.spaceMenu.preview, *next);
        }

    } // namespace

    void Blueprints::create(Writing context) {
        const auto grid_geometry = with<Assets>::find<geometry::Asset>(context, Unit::Name::from("rmmr", "grid"));
        const auto grid_material = with<Assets>::find<rmmr::resource::material::Asset>(context, Unit::Name::from("rmmr", "grid"));
        if (not grid_geometry or not grid_material)
            return (void)context.refuse("eltanin::views::Blueprints::create: grid assets missing");

        const auto kube_geometry = with<Assets>::find<geometry::Asset>(context, Unit::Name::from("rmmr", "kube"));
        const auto sphere_geometry = with<Assets>::find<geometry::Asset>(context, Unit::Name::from("rmmr", "sphere"));
        const auto cursor_material = with<Assets>::find<rmmr::resource::material::Asset>(context, Unit::Name::from("Eltanin", "type"));
        if (not kube_geometry or not sphere_geometry or not cursor_material)
            return (void)context.refuse("eltanin::views::Blueprints::create: kube / sphere / lit-transparent material missing");

        const auto device = with<World>::get_global(context).window;
        if (not device)
            return (void)context.refuse("eltanin::views::Blueprints::create: World window missing");

        const auto root = with<scene::Interface>::createScene(context);

        const float cell = mech::space::local::edge2meters;
        const float patternScale = 1.0f / cell;
        state.currentFloor = 0;
        state.cursorLattice = index3{.x = 0, .y = 0, .z = 0};
        state.mainScene.grid = with<scene::Interface>::createGrid(context, root, *device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Grid>{.geometry = *grid_geometry, .material = *grid_material, .opacity = gridOpacity, .patternScale = patternScale});

        const float edge = mech::space::local::edge2meters;
        const auto cursorResolved = meshpack::Asset::Resolved{
            .geometry = *kube_geometry,
            .entry = geometry::EntryId{0},
            .surfaces = {{geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = *cursor_material, .textures = {}}}},
            .texpack = {},
        };
        state.mainScene.worldCursor = with<scene::Interface>::createMeshActor(context, root, Pose::from(cellWorldPos(state.cursorLattice), HPB{0.0f, 0.0f, 0.0f}), cursorResolved, with<scene::actor::MeshState>::defaults(RGB{0.35f, 0.95f, 1.0f}, cursorOpacity, vec3{edge, edge, edge}));

        const Pos pivot{0.0f, 0.0f, 0.0f};
        const Pos camera_pos{24.0f, 20.0f, 40.0f};
        const auto camera = with<scene::Interface>::createCamera(context, root, Pose::from(camera_pos, HPB{36.87f, -29.74f, 0.0f}), 60.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::CameraOrbit>::create(context, camera, pivot, glm::length(camera_pos - pivot));
        applyOrbitPose(context, camera);

        with<scene::Interface>::createLight(context, root, Pose::from(Pos{9.5f, 19.0f, 7.5f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 120.0f});

        state.mainScene.root = root;
        state.mainScene.camera = camera;
        state.interframe = with<Assets>::find<meshpack::Asset>(context, Unit::Name::from("Eltanin", "interframe"));
        state.ghostMaterial = with<Assets>::find<::rmmr::resource::material::Asset>(context, Unit::Name::from("Eltanin", "clipboardGhost"));
        if (not state.ghostMaterial)
            return (void)context.refuse("eltanin::views::Blueprints::create: clipboardGhost material missing");
        state.mainScene.quarkActors = {};
        state.mainScene.clipboardActors = {};
        state.mainScene.mountActors = {};
        state.mainScene.clipboardMountActors = {};
        state.display = {.skeleton = true, .membranes = true, .internals = true, .externals = true, .floorMode = blueprints::geometry::Display::FloorMode::all};
        state.mountLayers = {};
        state.mountSpins = {};
        state.mountFits = {};
        state.cellBox.reset();
        state.editMode = EditMode::skeleton;
        state.membranes = {.enabled = false, .cell = {}, .slots = {}, .face = {}};
        state.mounts = {.enabled = false, .cell = {}, .face = {}, .points = {}, .balls = {}, .sphere = *sphere_geometry, .material = *cursor_material};
        blueprints::selection::clear(state.selection);
        blueprints::selection::resetClipboard(state.selection);
        blueprints::history::clear(state.history);
        state.hovered.reset();
        state.spaceMenu = {.place = false, .close = false};

        const auto paletteRoot = with<scene::Interface>::createScene(context);
        state.paletteScene.grid = with<scene::Interface>::createGrid(context, paletteRoot, *device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Grid>{.geometry = *grid_geometry, .material = *grid_material, .opacity = gridOpacity, .patternScale = patternScale});
        const Pos palettePivot{4.0f, 0.0f, 4.0f};
        const Pos paletteCameraPos{28.0f, 24.0f, 44.0f};
        const auto paletteCamera = with<scene::Interface>::createCamera(context, paletteRoot, Pose::from(paletteCameraPos, HPB{36.87f, -29.74f, 0.0f}), 60.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::CameraOrbit>::create(context, paletteCamera, palettePivot, glm::length(paletteCameraPos - palettePivot));
        applyOrbitPose(context, paletteCamera);
        with<scene::Interface>::createLight(context, paletteRoot, Pose::from(Pos{9.5f, 19.0f, 7.5f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 120.0f});
        state.paletteScene.root = paletteRoot;
        state.paletteScene.camera = paletteCamera;
        state.paletteScene.actors = {};
        state.paletteMode = false;
    }

    void Blueprints::show(Writing context, mech::Blueprint::Id asset_id) {
        state.hovered = asset_id;
        blueprints::history::bind(state.history, asset_id);
        blueprints::selection::clear(state.selection);
        syncVisuals(context);
        syncClipboardGhost(context);
        refreshMembraneCandidates(context);
    }

    void Blueprints::setEditMode(Writing context, EditMode mode) {
        if (state.editMode == mode)
            return;
        state.editMode = mode;
        state.membranes.enabled = mode == EditMode::membranes;
        state.mounts.enabled = mode == EditMode::mounts;
        blueprints::selection::clear(state.selection);
        closeSpaceMenu(context, state);
        if (mode != EditMode::mounts and state.mainScene.root.has_value()) {
            blueprints::mountPlacement::clearBalls(context, *state.mainScene.root, state.mounts);
            blueprints::mountPlacement::resetAim(state.mounts);
        }
        if (mode == EditMode::membranes and not state.display.membranes) {
            state.display.membranes = true;
            applyDisplay(context);
        }
        if (mode == EditMode::mounts and not state.display.internals and not state.display.externals) {
            state.display.internals = true;
            state.display.externals = true;
            applyDisplay(context);
        }
        refreshMembraneCandidates(context);
    }

    void Blueprints::syncVisuals(Writing context) {
        if (not state.mainScene.root.has_value() or not state.interframe.has_value())
            return;
        if (not state.hovered.has_value() or not with<::eltanin::mech::Blueprint>::exists(context, *state.hovered)) {
            blueprints::geometry::clearActors(context, *state.mainScene.root, state.mainScene.quarkActors);
            blueprints::geometry::clearActors(context, *state.mainScene.root, state.mainScene.clipboardActors);
            blueprints::geometry::clearMountActors(context, *state.mainScene.root, state.mainScene.mountActors);
            blueprints::geometry::clearMountActors(context, *state.mainScene.root, state.mainScene.clipboardMountActors);
            blueprints::selection::clear(state.selection);
            state.cellBox.reset();
            return;
        }
        const auto& data = with<::eltanin::mech::Blueprint>::get(context, *state.hovered);
        rebuildCellBox(context);
        blueprints::geometry::syncActors(context, *state.mainScene.root, *state.interframe, data, state.display, state.currentFloor, state.mainScene.quarkActors);
        blueprints::geometry::syncMountActors(context, *state.mainScene.root, data, state.display, state.currentFloor, state.mountLayers, state.mainScene.mountActors);
        blueprints::selection::rematchAfterSync(context, state.selection, state.mainScene.quarkActors, state.mainScene.mountActors);
    }

    void Blueprints::applyDisplay(Writing context) {
        blueprints::geometry::applyDisplay(context, state.display, state.currentFloor, state.mainScene.quarkActors, state.mainScene.mountActors);
    }

    void Blueprints::rebuildCellBox(Reading context) {
        state.cellBox.reset();
        if (not state.hovered.has_value() or not with<::eltanin::mech::Blueprint>::exists(context, *state.hovered))
            return;
        const auto& data = with<::eltanin::mech::Blueprint>::get(context, *state.hovered);
        blueprints::mountBounds::CellBox box{};
        bool any = false;
        for (const auto& cell : data.cells) {
            if (not any) {
                box = {.min = cell.placement.cell, .max = cell.placement.cell};
                any = true;
            } else {
                blueprints::mountBounds::include(box, cell.placement.cell);
            }
        }
        for (const auto& placed : data.mounts) {
            const auto mountId = with<::rmmr::resource::Assets>::find<::eltanin::mech::Mount>(context, placed.mount);
            if (not mountId)
                continue;
            const auto& mount = with<::eltanin::mech::Mount>::get(context, *mountId);
            const auto mountBox = blueprints::mountBounds::cellBox(mount.attachment, placed.transform);
            if (not mountBox)
                continue;
            if (not any) {
                box = *mountBox;
                any = true;
            } else {
                blueprints::mountBounds::include(box, *mountBox);
            }
        }
        if (any)
            state.cellBox = box;
    }

    void Blueprints::syncClipboardGhost(Writing context) {
        if (not state.mainScene.root.has_value() or not state.interframe.has_value() or not state.ghostMaterial.has_value())
            return;
        if (blueprints::selection::clipboardEmpty(state.selection)) {
            blueprints::geometry::clearActors(context, *state.mainScene.root, state.mainScene.clipboardActors);
            blueprints::geometry::clearMountActors(context, *state.mainScene.root, state.mainScene.clipboardMountActors);
            return;
        }
        constexpr float ghostOpacity = 0.45f;
        const RGB okTint{0.22f, 0.55f, 0.62f};
        const RGB blockedTint{0.62f, 0.22f, 0.18f};
        bool allowed = false;
        if (state.hovered.has_value() and with<::eltanin::mech::Blueprint>::exists(context, *state.hovered))
            allowed = blueprints::selection::canPaste(with<::eltanin::mech::Blueprint>::get(context, *state.hovered), state.selection.clipboard);
        const auto tint = allowed ? okTint : blockedTint;
        const auto& clipboard = state.selection.clipboard;
        if (clipboard.cells.empty()) {
            blueprints::geometry::clearActors(context, *state.mainScene.root, state.mainScene.clipboardActors);
        } else if (not blueprints::geometry::refreshGhostActors(context, *state.interframe, clipboard, state.display, state.mainScene.clipboardActors, tint, ghostOpacity)) {
            blueprints::geometry::syncGhostActors(context, *state.mainScene.root, *state.interframe, *state.ghostMaterial, clipboard, state.display, state.mainScene.clipboardActors, tint, ghostOpacity);
        }
        if (clipboard.mounts.empty()) {
            blueprints::geometry::clearMountActors(context, *state.mainScene.root, state.mainScene.clipboardMountActors);
        } else if (not blueprints::geometry::refreshGhostMountActors(context, clipboard, state.display, state.mainScene.clipboardMountActors, tint, ghostOpacity)) {
            blueprints::geometry::syncGhostMountActors(context, *state.mainScene.root, *state.ghostMaterial, clipboard, state.display, state.mainScene.clipboardMountActors, tint, ghostOpacity);
        }
    }

    void Blueprints::rebuildMountLayers(Reading context, MountCatalog& mounts) {
        state.mountLayers.clear();
        state.mountSpins.clear();
        state.mountFits.clear();
        state.mountLayers.reserve(mounts.ids.size());
        state.mountSpins.reserve(mounts.ids.size());
        state.mountFits.reserve(mounts.ids.size());
        for (const auto id : mounts.ids) {
            if (not with<::eltanin::mech::Mount>::exists(context, id))
                continue;
            const auto& mount = with<::eltanin::mech::Mount>::get(context, id);
            const auto layer = mount.attachment.flatMounted() ? mech::Layer::externals : mech::Layer::internals;
            state.mountLayers.emplace(id, layer);
            state.mountSpins.emplace(id, blueprints::mountEditor::buildSpins(mount.attachment));
            state.mountFits.emplace(id, blueprints::mountEditor::buildFits(mount.attachment));
        }
    }

    void Blueprints::syncPalette(Writing context, MountCatalog& mounts) {
        if (not state.paletteScene.root.has_value())
            return;
        if (not state.mounts.sphere.has_value() or not state.mounts.material.has_value())
            return;
        if (state.mountLayers.empty() and not mounts.ids.empty())
            rebuildMountLayers(context, mounts);
        blueprints::geometry::syncPaletteActors(context, *state.paletteScene.root, mounts.ids, state.paletteScene.actors, *state.mounts.sphere, *state.mounts.material);
    }

    void Blueprints::refreshMembraneCandidates(Reading context) {
        state.membranes.face = {};
        state.membranes.cell = {};
        state.membranes.slots = {};
        if (not state.membranes.enabled)
            return;
        aimMembraneTarget(context);
    }

    void Blueprints::aimMembraneTarget(Reading context) {
        state.membranes.face = {};
        state.membranes.cell = {};
        state.membranes.slots = {};
        if (not state.membranes.enabled or ImGui::GetIO().WantCaptureMouse)
            return;
        if (not state.hovered.has_value() or not with<::eltanin::mech::Blueprint>::exists(context, *state.hovered))
            return;
        if (not state.mainScene.camera.has_value())
            return;
        const auto window = firstWindow(context);
        const auto viewport = firstViewport(context);
        if (not window.has_value() or not viewport.has_value())
            return;
        const auto mouse = with<system::Window>::get(context, *window).current.mouse;
        const auto ray = mouseRay(context, *state.mainScene.camera, *viewport, mouse);
        if (not ray.has_value())
            return;
        const auto& data = with<::eltanin::mech::Blueprint>::get(context, *state.hovered);
        base::maybe<std::size_t> bestCell;
        base::maybe<std::size_t> bestSlot;
        float bestFreeT = std::numeric_limits<float>::infinity();
        float bestPlacedT = std::numeric_limits<float>::infinity();
        for (std::size_t cellIndex = 0; cellIndex < data.cells.size(); ++cellIndex) {
            const auto& cell = data.cells[cellIndex];
            if (not state.display.spatialOk(cell.placement.cell.y, cell.placement.cell.y, state.currentFloor))
                continue;
            for (const auto& placed : cell.membranes) {
                if (const auto face = blueprints::membraneSlots::faceFor(cell, placed)) {
                    const auto loop = faceWorldLoop(cell, *face);
                    float t = 0.0f;
                    if (rayHitPolygon(ray->origin, ray->dir, loop, t) and t < bestPlacedT)
                        bestPlacedT = t;
                }
            }
            const auto slots = blueprints::membraneSlots::possible(cell);
            for (std::size_t slot = 0; slot < slots.size(); ++slot) {
                const auto loop = faceWorldLoop(cell, slots[slot].face);
                float t = 0.0f;
                if (rayHitPolygon(ray->origin, ray->dir, loop, t) and t < bestFreeT) {
                    bestFreeT = t;
                    bestCell = cellIndex;
                    bestSlot = slot;
                }
            }
        }
        if (not bestCell.has_value() or not bestSlot.has_value() or bestPlacedT <= bestFreeT)
            return;
        state.membranes.cell = *bestCell;
        state.membranes.slots = blueprints::membraneSlots::possible(data.cells[*bestCell]);
        if (*bestSlot < state.membranes.slots.size())
            state.membranes.face = *bestSlot;
    }

    void Blueprints::drawMembraneFaceHighlight(Reading context) const {
        if (not state.membranes.enabled or not state.membranes.cell.has_value() or not state.membranes.face.has_value())
            return;
        if (not state.hovered.has_value() or not with<::eltanin::mech::Blueprint>::exists(context, *state.hovered))
            return;
        if (not state.mainScene.camera.has_value())
            return;
        const auto viewport = firstViewport(context);
        if (not viewport.has_value())
            return;
        if (*state.membranes.face >= state.membranes.slots.size())
            return;
        const auto& data = with<::eltanin::mech::Blueprint>::get(context, *state.hovered);
        if (*state.membranes.cell >= data.cells.size())
            return;
        const auto loop = faceWorldLoop(data.cells[*state.membranes.cell], state.membranes.slots[*state.membranes.face].face);
        if (loop.size() < 2)
            return;
        std::vector<ImVec2> pts;
        pts.reserve(loop.size() + 1);
        for (const auto& world : loop) {
            const auto screen = projectWorld(context, *state.mainScene.camera, *viewport, world);
            if (not screen.has_value())
                return;
            pts.push_back(*screen);
        }
        pts.push_back(pts.front());
        auto* draw = ImGui::GetForegroundDrawList();
        draw->AddPolyline(pts.data(), static_cast<int>(pts.size()), IM_COL32(255, 180, 64, 220), ImDrawFlags_None, 2.5f);
    }

    void Blueprints::aimMountCursor(Reading context, renderer::Integer32 under) {
        if (not state.mounts.enabled or state.paletteMode) {
            blueprints::mountPlacement::resetAim(state.mounts);
            return;
        }
        // Keep last aim while ImGui owns the mouse (selection panel / etc.).
        if (ImGui::GetIO().WantCaptureMouse)
            return;
        // Pointer over a placed mount → hover/select owns the cursor; no face aim this frame.
        if (blueprints::selection::hitMount(context, state.mainScene.mountActors, under)) {
            blueprints::mountPlacement::resetAim(state.mounts);
            return;
        }
        blueprints::mountPlacement::resetAim(state.mounts);
        if (not state.hovered.has_value() or not with<::eltanin::mech::Blueprint>::exists(context, *state.hovered))
            return;
        if (not state.mainScene.camera.has_value())
            return;
        const auto window = firstWindow(context);
        const auto viewport = firstViewport(context);
        if (not window.has_value() or not viewport.has_value())
            return;
        const auto mouse = with<system::Window>::get(context, *window).current.mouse;
        const auto ray = mouseRay(context, *state.mainScene.camera, *viewport, mouse);
        if (not ray.has_value())
            return;
        const auto& data = with<::eltanin::mech::Blueprint>::get(context, *state.hovered);
        blueprints::mountPlacement::aim(state.mounts, data, blueprints::mountPlacement::MouseRay{.origin = ray->origin, .dir = ray->dir}, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyAlt);
    }

    void Blueprints::syncMountCursor(Writing context, renderer::Integer32 under) {
        if (not state.mainScene.root.has_value())
            return;
        if (not state.mounts.enabled) {
            blueprints::mountPlacement::clearBalls(context, *state.mainScene.root, state.mounts);
            blueprints::mountPlacement::resetAim(state.mounts);
            return;
        }
        aimMountCursor(context, under);
        blueprints::mountPlacement::syncBalls(context, *state.mainScene.root, state.mounts);
    }

    auto Blueprints::handleMembraneMode(Writing context, renderer::Integer32 under) -> bool {
        if (not state.membranes.enabled or ImGui::GetIO().WantCaptureMouse)
            return false;
        if (not state.hovered.has_value() or not with<::eltanin::mech::Blueprint>::exists(context, *state.hovered))
            return false;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (not state.membranes.cell.has_value() or not state.membranes.face.has_value() or state.membranes.slots.empty())
                return false;
            if (*state.membranes.face >= state.membranes.slots.size())
                return false;
            const auto cell = *state.membranes.cell;
            const auto membrane = state.membranes.slots[*state.membranes.face].membrane;
            if (cell >= with<::eltanin::mech::Blueprint>::get(context, *state.hovered).cells.size())
                return false;
            blueprints::history::record(state.history, *state.hovered, "place membrane", with<::eltanin::mech::Blueprint>::get(context, *state.hovered));
            auto data = with<::eltanin::mech::Blueprint>::modify(context, *state.hovered);
            auto& membranes = data->cells[cell].membranes;
            membranes.push_back(membrane);
            const auto membraneIndex = membranes.size() - 1;
            const auto placement = data->cells[cell].placement;
            persistHovered(context);
            if (state.mainScene.root.has_value() and state.interframe.has_value())
                blueprints::geometry::appendWallActor(context, *state.mainScene.root, *state.interframe, cell, membraneIndex, placement, membrane, state.display, state.currentFloor, state.mainScene.quarkActors);
            refreshMembraneCandidates(context);
            return true;
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (under == renderer::Integer32{0})
                return false;
            for (std::size_t slot = 0; slot < state.mainScene.quarkActors.size(); ++slot) {
                const auto& actor = state.mainScene.quarkActors[slot];
                if (actor.kind != blueprints::geometry::QuarkActor::Kind::wall)
                    continue;
                if (not with<scene::actor::Identified>::exists(context, actor.id))
                    continue;
                if (with<scene::actor::Identified>::get(context, actor.id).scenicAlias != under)
                    continue;
                if (actor.cell >= with<::eltanin::mech::Blueprint>::get(context, *state.hovered).cells.size())
                    return true;
                {
                    const auto& view = with<::eltanin::mech::Blueprint>::get(context, *state.hovered);
                    if (actor.index >= view.cells[actor.cell].membranes.size())
                        return true;
                }
                blueprints::history::record(state.history, *state.hovered, "remove membrane", with<::eltanin::mech::Blueprint>::get(context, *state.hovered));
                auto data = with<::eltanin::mech::Blueprint>::modify(context, *state.hovered);
                auto& membranes = data->cells[actor.cell].membranes;
                membranes.erase(membranes.begin() + static_cast<std::ptrdiff_t>(actor.index));
                persistHovered(context);
                if (state.mainScene.root.has_value())
                    blueprints::geometry::eraseWallActor(context, *state.mainScene.root, slot, state.mainScene.quarkActors);
                refreshMembraneCandidates(context);
                return true;
            }
            return false;
        }
        return false;
    }

    void Blueprints::syncGridToFloor(Writing context) {
        if (not state.mainScene.grid.has_value() or not with<scene::Node>::exists(context, *state.mainScene.grid))
            return;
        const float cell = mech::space::local::edge2meters;
        const float y = static_cast<float>(state.currentFloor) * cell;
        with<scene::Node>::modify(context, *state.mainScene.grid)->pose = Pose::from(Pos{0.0f, y, 0.0f}, HPB{0.0f, 0.0f, 0.0f});
        if (state.mainScene.camera.has_value() and with<controller::CameraOrbit>::exists(context, *state.mainScene.camera)) {
            auto orbit = with<controller::CameraOrbit>::modify(context, *state.mainScene.camera);
            orbit->pivot.y = static_cast<float>(state.currentFloor) * cell;
            applyOrbitPose(context, *state.mainScene.camera);
        }
    }

    void Blueprints::updateWorldCursor(Writing context) {
        if (not state.mainScene.worldCursor.has_value() or not with<scene::Node>::exists(context, *state.mainScene.worldCursor))
            return;

        if (with<scene::actor::MeshState>::exists(context, *state.mainScene.worldCursor))
            scene::actor::MeshState::Actions::setVisible(context, *state.mainScene.worldCursor, not state.membranes.enabled and not state.mounts.enabled);

        if (state.membranes.enabled or state.mounts.enabled)
            return;

        const float cell = mech::space::local::edge2meters;
        const float planeY = static_cast<float>(state.currentFloor) * cell;
        auto lattice = index3{.x = state.cursorLattice.x, .y = state.currentFloor, .z = state.cursorLattice.z};

        if (state.mainScene.camera.has_value()) {
            if (const auto window = firstWindow(context); window and not ImGui::GetIO().WantCaptureMouse) {
                if (const auto viewport = firstViewport(context)) {
                    const auto mouse = with<system::Window>::get(context, *window).current.mouse;
                    if (const auto hit = rayHitFloor(context, *state.mainScene.camera, *viewport, mouse, planeY)) {
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
        with<scene::Node>::modify(context, *state.mainScene.worldCursor)->pose = Pose::from(cellWorldPos(state.cursorLattice), HPB{0.0f, 0.0f, 0.0f});
    }

    void Blueprints::persistHovered(Writing context) {
        if (not state.hovered.has_value() or not with<::eltanin::mech::Blueprint>::exists(context, *state.hovered))
            return;
        with<::eltanin::mech::Blueprint>::save(context, *state.hovered);
    }

    void Blueprints::applyHistory(Writing context, blueprints::history::UiAction action) {
        if (action == blueprints::history::UiAction::none or not state.hovered.has_value())
            return;
        const bool ok = action == blueprints::history::UiAction::undo
            ? blueprints::history::undo(context, state.history, *state.hovered)
            : blueprints::history::redo(context, state.history, *state.hovered);
        if (not ok)
            return;
        blueprints::selection::clear(state.selection);
        syncVisuals(context);
        syncClipboardGhost(context);
        refreshMembraneCandidates(context);
        if (state.mounts.enabled)
            syncMountCursor(context, renderer::Integer32{0});
    }

    void Blueprints::draw(Writing context, bool& open, BlueprintCatalog& catalog, MountCatalog& mounts) {
        if (not open) {
            if (state.mainScene.root.has_value()) {
                blueprints::geometry::clearActors(context, *state.mainScene.root, state.mainScene.clipboardActors);
                blueprints::geometry::clearMountActors(context, *state.mainScene.root, state.mainScene.clipboardMountActors);
            }
            return;
        }

        if (mounts.ready and state.mountLayers.empty() and not mounts.ids.empty())
            rebuildMountLayers(context, mounts);

        if (mounts.ready and state.paletteMode and state.paletteScene.actors.empty() and not mounts.ids.empty())
            syncPalette(context, mounts);

        if (not state.paletteMode) {
            if (ImGui::IsKeyPressed(ImGuiKey_F1))
                setEditMode(context, EditMode::skeleton);
            if (ImGui::IsKeyPressed(ImGuiKey_F2))
                setEditMode(context, EditMode::membranes);
            if (ImGui::IsKeyPressed(ImGuiKey_F3))
                setEditMode(context, EditMode::mounts);
        }

        renderer::Integer32 under = renderer::Integer32{0};
        for (const auto [_, window] : context->aspect<system::Window>().items()) {
            under = window.current.under;
            break;
        }

        if (not state.paletteMode and not ImGui::GetIO().WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
                ++state.currentFloor;
                syncGridToFloor(context);
                applyDisplay(context);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
                --state.currentFloor;
                syncGridToFloor(context);
                applyDisplay(context);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_B) and not state.membranes.enabled)
                blueprints::selection::toggleFocus(state.selection);
            if (ImGui::GetIO().KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_Z))
                    applyHistory(context, blueprints::history::UiAction::undo);
                if (ImGui::IsKeyPressed(ImGuiKey_Y))
                    applyHistory(context, blueprints::history::UiAction::redo);
            }
        }

        if (not state.paletteMode) {
        updateWorldCursor(context);
        if (state.mounts.enabled) {
            syncMountCursor(context, under);
        } else if (state.mainScene.root.has_value()) {
            blueprints::mountPlacement::clearBalls(context, *state.mainScene.root, state.mounts);
        }
        if (state.membranes.enabled)
            aimMembraneTarget(context);

        if (state.mounts.enabled) {
            // Mount-only pick: quarks ignored (empty quark list). Face aim balls stay separate.
            blueprints::selection::handlePointer(context, state.selection, state.hovered, {}, state.mainScene.mountActors, under);
            if (blueprints::selection::handleHotkeys(context, state.selection, state.history, state.hovered, {}, state.mainScene.mountActors)) {
                persistHovered(context);
                syncVisuals(context);
                syncClipboardGhost(context);
                syncMountCursor(context, under);
            }
            blueprints::selection::handleClipboardHotkeys(state.selection);
            if (blueprints::selection::handleClipboardChords(context, state.selection, state.history, state.hovered, {}, state.mainScene.mountActors)) {
                persistHovered(context);
                syncVisuals(context);
                syncClipboardGhost(context);
                syncMountCursor(context, under);
            }
            if (state.hovered.has_value() and with<::eltanin::mech::Blueprint>::exists(context, *state.hovered)) {
                if (const auto mountIndex = blueprints::selection::soleSelectedMountIndex(context, state.selection, state.mainScene.mountActors)) {
                    if (*mountIndex < with<::eltanin::mech::Blueprint>::get(context, *state.hovered).mounts.size()) {
                        const auto placed = with<::eltanin::mech::Blueprint>::get(context, *state.hovered).mounts[*mountIndex];
                        const auto mountId = with<Assets>::find<::eltanin::mech::Mount>(context, placed.mount);
                        if (mountId) {
                            if (not state.mountReplace.mountIndex.has_value() or *state.mountReplace.mountIndex != *mountIndex) {
                                state.mountReplace.mountIndex = *mountIndex;
                                state.mountReplace.alternatives.clear();
                                const auto& currentMount = with<::eltanin::mech::Mount>::get(context, *mountId);
                                const auto& unit = with<Unit>::get(context, *mountId);
                                state.mountReplace.original = blueprints::mountEditor::ReplaceOption{
                                    .name = placed.mount,
                                    .label = currentMount.name.empty() ? unit.name.own : currentMount.name,
                                };
                                const auto footprint = blueprints::mountEditor::worldPoints(currentMount.attachment, placed.transform);
                                for (const auto id : mounts.ids) {
                                    if (id == *mountId)
                                        continue;
                                    const auto found = state.mountFits.find(id);
                                    if (found == state.mountFits.end() or not blueprints::mountEditor::matchesCursor(found->second, footprint))
                                        continue;
                                    if (not with<::eltanin::mech::Mount>::exists(context, id))
                                        continue;
                                    const auto& alt = with<::eltanin::mech::Mount>::get(context, id);
                                    const auto& altUnit = with<Unit>::get(context, id);
                                    state.mountReplace.alternatives.push_back(blueprints::mountEditor::ReplaceOption{
                                        .name = altUnit.name,
                                        .label = alt.name.empty() ? altUnit.name.own : alt.name,
                                    });
                                }
                            }
                            bool visualsDirty = false;
                            if (const auto found = state.mountSpins.find(*mountId); found != state.mountSpins.end()) {
                                if (const auto chosen = blueprints::mountEditor::drawOriMenu(placed.transform.rotation, found->second)) {
                                    if (const auto next = blueprints::mountEditor::applyOri(placed.transform, found->second, *chosen)) {
                                        if (blueprints::selection::setSoleMountTransform(context, state.selection, state.history, *state.hovered, state.mainScene.mountActors, *next))
                                            visualsDirty = true;
                                    }
                                }
                            }
                            // Swap catalog unit only — keep current grid/rotation (seatingOn re-picked a "default" ori and undid flips).
                            if (const auto picked = blueprints::mountEditor::drawReplaceMenu(placed.mount, state.mountReplace.original, state.mountReplace.alternatives)) {
                                if (blueprints::selection::setSoleMountUnit(context, state.selection, state.history, *state.hovered, state.mainScene.mountActors, *picked, {}))
                                    visualsDirty = true;
                            }
                            if (visualsDirty) {
                                persistHovered(context);
                                syncVisuals(context);
                            }
                        }
                    }
                } else {
                    state.mountReplace = {};
                }
            } else {
                state.mountReplace = {};
            }
        } else if (state.membranes.enabled) {
            handleMembraneMode(context, under);
        } else {
            blueprints::selection::handlePointer(context, state.selection, state.hovered, state.mainScene.quarkActors, state.mainScene.mountActors, under);
            if (blueprints::selection::handleHotkeys(context, state.selection, state.history, state.hovered, state.mainScene.quarkActors, state.mainScene.mountActors)) {
                persistHovered(context);
                syncVisuals(context);
                syncClipboardGhost(context);
                refreshMembraneCandidates(context);
            }
            blueprints::selection::handleClipboardHotkeys(state.selection);
            if (blueprints::selection::handleClipboardChords(context, state.selection, state.history, state.hovered, state.mainScene.quarkActors, state.mainScene.mountActors)) {
                persistHovered(context);
                syncVisuals(context);
                refreshMembraneCandidates(context);
            }
        }

        constexpr auto spaceMenuPopup = "##blueprints.spaceMenu";
        const bool spaceMenuOpen = ImGui::IsPopupOpen(spaceMenuPopup);
        const bool spaceMenuActive = state.spaceMenu.place;
        const bool f3Place = state.editMode == EditMode::mounts and state.selection.aliases.empty();
        const bool spaceOk = state.editMode == EditMode::skeleton or f3Place;
        if (not spaceOk) {
            if (spaceMenuOpen or spaceMenuActive)
                closeSpaceMenu(context, state);
        } else if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
            if (spaceMenuOpen or spaceMenuActive) {
                state.spaceMenu.close = true;
            } else if (not ImGui::GetIO().WantCaptureKeyboard) {
                closeSpaceMenu(context, state);
                state.spaceMenu.place = true;
                ImGui::OpenPopup(spaceMenuPopup);
                ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
            }
        }

        if (state.editMode == EditMode::skeleton and spaceMenuActive) {
            if (ImGui::BeginPopup(spaceMenuPopup)) {
                if (state.spaceMenu.close) {
                    ImGui::CloseCurrentPopup();
                    closeSpaceMenu(context, state);
                } else if (not state.hovered.has_value() or not with<::eltanin::mech::Blueprint>::exists(context, *state.hovered)) {
                    ImGui::TextDisabled("No blueprint");
                } else {
                    const auto& dataView = with<::eltanin::mech::Blueprint>::get(context, *state.hovered);
                    const bool occupied = blueprints::selection::cellOccupied(dataView, state.cursorLattice);
                    if (occupied) {
                        if (ImGui::Selectable("erase")) {
                            blueprints::history::record(state.history, *state.hovered, "erase cell", dataView);
                            {
                                auto data = with<::eltanin::mech::Blueprint>::modify(context, *state.hovered);
                                data->cells.erase(std::remove_if(data->cells.begin(), data->cells.end(), [&](const Cell& cell) { return blueprints::selection::sameIndex3(cell.placement.cell, state.cursorLattice); }), data->cells.end());
                            }
                            ImGui::CloseCurrentPopup();
                            closeSpaceMenu(context, state);
                            blueprints::selection::clear(state.selection);
                            persistHovered(context);
                            syncVisuals(context);
                        }
                    } else {
                        bool applied = false;
                        applied = frameShapePicks([&](mech::frame::shape shape) {
                            blueprints::history::record(state.history, *state.hovered, "spawn cell", with<::eltanin::mech::Blueprint>::get(context, *state.hovered));
                            auto data = with<::eltanin::mech::Blueprint>::modify(context, *state.hovered);
                            const auto pose = mech::space::cell::Placement{.cell = state.cursorLattice, .ori = 0};
                            data->cells.push_back(Cell{
                                .placement = pose,
                                .shape = shape,
                                .corners = mech::skeleton::seedCorners(shape),
                                .halfribs = mech::skeleton::seedHalfribs(shape),
                                .membranes = {},
                            });
                        });
                        if (applied) {
                            ImGui::CloseCurrentPopup();
                            closeSpaceMenu(context, state);
                            blueprints::selection::clear(state.selection);
                            persistHovered(context);
                            syncVisuals(context);
                        }
                    }
                }
                ImGui::EndPopup();
            } else {
                closeSpaceMenu(context, state);
            }
        }

        if (f3Place and spaceMenuActive) {
            if (ImGui::BeginPopup(spaceMenuPopup)) {
                if (state.spaceMenu.close) {
                    ImGui::CloseCurrentPopup();
                    closeSpaceMenu(context, state);
                } else if (not state.hovered.has_value() or not with<::eltanin::mech::Blueprint>::exists(context, *state.hovered)) {
                    ImGui::TextDisabled("No blueprint");
                    dropSpacePreview(context, state);
                } else if (state.mounts.points.empty()) {
                    ImGui::TextDisabled("Aim a cell face");
                    dropSpacePreview(context, state);
                } else {
                    ImGui::TextDisabled("%zu pts · matching mounts", state.mounts.points.size());
                    ImGui::Separator();
                    bool any = false;
                    base::maybe<mech::Mount::Id> hoveredMount;
                    for (const auto id : mounts.ids) {
                        const auto found = state.mountFits.find(id);
                        if (found == state.mountFits.end() or not blueprints::mountEditor::matchesCursor(found->second, state.mounts.points))
                            continue;
                        if (not with<::eltanin::mech::Mount>::exists(context, id))
                            continue;
                        const auto& mount = with<::eltanin::mech::Mount>::get(context, id);
                        const auto& unit = with<Unit>::get(context, id);
                        const char* label = mount.name.empty() ? unit.name.own.c_str() : mount.name.c_str();
                        any = true;
                        ImGui::PushID(reinterpret_cast<const void*>(id.raw()));
                        if (ImGui::Selectable(label)) {
                            base::maybe<mech::space::Transform> seating;
                            if (state.spaceMenu.previewMount.has_value() and *state.spaceMenu.previewMount == id and state.spaceMenu.previewTransform.has_value())
                                seating = state.spaceMenu.previewTransform;
                            else
                                seating = blueprints::mountEditor::seatingOn(mount.attachment, found->second, state.mounts.points);
                            if (seating) {
                                blueprints::history::record(state.history, *state.hovered, "place mount", with<::eltanin::mech::Blueprint>::get(context, *state.hovered));
                                auto writable = with<::eltanin::mech::Blueprint>::modify(context, *state.hovered);
                                writable->mounts.push_back(mech::Blueprint::Mounted{.mount = unit.name, .transform = *seating});
                                ImGui::CloseCurrentPopup();
                                closeSpaceMenu(context, state);
                                persistHovered(context);
                                syncVisuals(context);
                            }
                        }
                        if (ImGui::IsItemHovered() or ImGui::IsItemFocused())
                            hoveredMount = id;
                        ImGui::PopID();
                    }
                    if (not any)
                        ImGui::TextDisabled("no catalog mount for this footprint");
                    if (state.spaceMenu.place and hoveredMount.has_value() and with<::eltanin::mech::Mount>::exists(context, *hoveredMount)) {
                        const auto found = state.mountFits.find(*hoveredMount);
                        if (found != state.mountFits.end())
                            ensurePlacePreview(context, state, *hoveredMount, with<::eltanin::mech::Mount>::get(context, *hoveredMount).attachment, found->second);
                    }
                    if (state.spaceMenu.place)
                        applyPlacePreviewOri(context, state);
                }
                ImGui::EndPopup();
            } else {
                closeSpaceMenu(context, state);
            }
        }
        } // not paletteMode

        bool shown = open;
        ImVec2 blueprintsPos{};
        ImVec2 blueprintsSize{};
        if (ImGui::Begin("Blueprints", &shown)) {
            blueprintsPos = ImGui::GetWindowPos();
            blueprintsSize = ImGui::GetWindowSize();
            if (ImGui::Checkbox("Mount palette", &state.paletteMode)) {
                if (state.paletteMode)
                    syncPalette(context, mounts);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%zu catalog", mounts.ids.size());
            ImGui::Separator();
            ImGui::BeginChild("blueprintList", ImVec2{220.0f, 0.0f}, true);
            const auto pickBlueprint = [&](mech::Blueprint::Id assetId) {
                const auto& unit = with<Unit>::get(context, assetId);
                const auto& asset = with<::eltanin::mech::Blueprint>::get(context, assetId);
                const bool hovered = state.hovered.has_value() and *state.hovered == assetId;
                const char* label = asset.name.empty() ? unit.name.own.c_str() : asset.name.c_str();
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
            if (catalog.unnamed and with<::eltanin::mech::Blueprint>::exists(context, *catalog.unnamed))
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
            if (not state.hovered.has_value() or not with<::eltanin::mech::Blueprint>::exists(context, *state.hovered)) {
                ImGui::TextDisabled("Select a blueprint.");
            } else {
                const auto& unit = with<Unit>::get(context, *state.hovered);
                const auto& data = with<::eltanin::mech::Blueprint>::get(context, *state.hovered);
                ImGui::Text("File: %s", unit.name.own.c_str());
                ImGui::Text("Name: %s", data.name.c_str());
                ImGui::Text("Manufacturer: %s", data.author.c_str());
                ImGui::Separator();
                bool displayChanged = false;
                displayChanged |= ImGui::Checkbox("Skeleton", &state.display.skeleton);
                ImGui::SameLine();
                displayChanged |= ImGui::Checkbox("Membranes", &state.display.membranes);
                ImGui::SameLine();
                displayChanged |= ImGui::Checkbox("Internals", &state.display.internals);
                ImGui::SameLine();
                displayChanged |= ImGui::Checkbox("Externals", &state.display.externals);
                {
                    const char* floorLabels[] = {"All floors", "Not above current", "Only current"};
                    auto floorMode = static_cast<int>(state.display.floorMode);
                    if (ImGui::Combo("Floors", &floorMode, floorLabels, 3)) {
                        state.display.floorMode = static_cast<blueprints::geometry::Display::FloorMode>(floorMode);
                        displayChanged = true;
                    }
                }
                if (displayChanged) {
                    applyDisplay(context);
                    syncClipboardGhost(context);
                }
                {
                    const char* modeLabels[] = {"Skeleton (F1)", "Membranes (F2)", "Mounts (F3)"};
                    auto mode = static_cast<int>(state.editMode);
                    if (ImGui::Combo("Mode", &mode, modeLabels, 3))
                        setEditMode(context, static_cast<EditMode>(mode));
                }
                if (state.mounts.enabled) {
                    ImGui::TextDisabled("ray aim · balls · LMB/RMB select · Space place · ori menu");
                    if (state.mounts.points.empty()) {
                        ImGui::TextDisabled("Mount cursor: aim a free face (or select one mount)");
                    } else {
                        ImGui::Text("Mount cursor: %zu grid pts", state.mounts.points.size());
                        for (const auto& point : state.mounts.points)
                            ImGui::Text("  [%d, %d, %d]", point.x, point.y, point.z);
                    }
                }
                if (state.membranes.enabled) {
                    ImGui::TextDisabled("ray aim · LMB place · RMB remove · selection off");
                    if (not state.membranes.cell.has_value()) {
                        ImGui::TextDisabled("Membrane: aim a free face");
                    } else if (state.membranes.slots.empty()) {
                        ImGui::TextDisabled("Membrane: no free faces on hit cell");
                    } else if (not state.membranes.face.has_value()) {
                        ImGui::TextDisabled("Membrane: %zu free on cell", state.membranes.slots.size());
                    } else {
                        const auto& slot = state.membranes.slots[*state.membranes.face];
                        const auto codeIt = mech::skeleton::membraneSpecs.find(slot.membrane.kind);
                        const auto code = codeIt != mech::skeleton::membraneSpecs.end() ? codeIt->second.code : "?";
                        const auto& pos = data.cells[*state.membranes.cell].placement.cell;
                        ImGui::Text("Membrane [%d,%d,%d]: %.*s · ori %d", pos.x, pos.y, pos.z, static_cast<int>(code.size()), code.data(), static_cast<int>(slot.membrane.ori));
                    }
                }
                if (state.editMode == EditMode::skeleton)
                    ImGui::TextDisabled("select knots/half-chords · Space spawn · clipboard");
                ImGui::Separator();
                std::size_t knots = 0;
                std::size_t halfChords = 0;
                std::size_t membranes = 0;
                for (const auto& cell : data.cells) {
                    knots += cell.corners.size();
                    halfChords += cell.halfribs.size();
                    membranes += cell.membranes.size();
                }
                ImGui::Text("Cells: %zu", data.cells.size());
                ImGui::Text("Knots: %zu", knots);
                ImGui::Text("Half-chords: %zu", halfChords);
                ImGui::Text("Membranes: %zu", membranes);
                ImGui::Text("Mounts: %zu", data.mounts.size());
                ImGui::Text("Actors: %zu", state.mainScene.quarkActors.size() + state.mainScene.mountActors.size());
                if (state.paletteMode)
                    ImGui::Text("Palette actors: %zu", state.paletteScene.actors.size());
                ImGui::Text("Selection: %zu", state.selection.aliases.size());
                if (state.membranes.enabled) {
                    if (state.membranes.cell.has_value() and *state.membranes.cell < data.cells.size()) {
                        const auto& pos = data.cells[*state.membranes.cell].placement.cell;
                        ImGui::Text("Target [%d, %d, %d]", pos.x, pos.y, pos.z);
                    } else {
                        ImGui::TextDisabled("Target: —");
                    }
                } else if (state.mounts.enabled) {
                    ImGui::Text("Aim points: %zu", state.mounts.points.size());
                } else {
                    ImGui::Text("Cursor [%d, %d, %d]", state.cursorLattice.x, state.cursorLattice.y, state.cursorLattice.z);
                    ImGui::Text("Floor: %d", state.currentFloor);
                }
                if (state.cellBox.has_value()) {
                    const auto& box = *state.cellBox;
                    ImGui::Text("Cell box Y: [%d, %d]  (%d floors)", box.min.y, box.max.y, box.max.y - box.min.y + 1);
                } else {
                    ImGui::TextDisabled("Cell box: —");
                }
                ImGui::TextDisabled("F1/F2/F3 mode · MMB orbit · PgUp/PgDn · Space · LMB/RMB±Shift · Del · WASD/QE · Shift rotate · Ctrl+C/V · Ctrl+Z/Y · B · F3: Ctrl=Kn · Alt=p121 on square");
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

        if (not state.paletteMode and not state.membranes.enabled) {
            static const std::vector<blueprints::geometry::QuarkActor> noQuarks;
            const auto& quarksForSelect = state.mounts.enabled ? noQuarks : state.mainScene.quarkActors;
            if (blueprints::selection::drawPanel(context, state.selection, state.history, blueprintsPos, blueprintsSize, state.hovered, quarksForSelect, state.mainScene.mountActors)) {
                persistHovered(context);
                syncVisuals(context);
                refreshMembraneCandidates(context);
            }
            if (blueprints::selection::drawClipboardPanel(context, state.selection, state.history, blueprintsPos, blueprintsSize, state.hovered)) {
                persistHovered(context);
                syncVisuals(context);
                refreshMembraneCandidates(context);
            }
            syncClipboardGhost(context);
        } else if (not state.paletteMode and state.mainScene.root.has_value()) {
            blueprints::geometry::clearActors(context, *state.mainScene.root, state.mainScene.clipboardActors);
            blueprints::geometry::clearMountActors(context, *state.mainScene.root, state.mainScene.clipboardMountActors);
        }
        if (not state.paletteMode)
            applyHistory(context, blueprints::history::drawWindow(state.history));
        if (not state.paletteMode and not state.mounts.enabled)
            drawMembraneFaceHighlight(context);
    }

    void Blueprints::bindView(std::vector<rmmr::wrapper::Product::View>& product_views, bool open, const rmmr::wrapper::Product::View& world_view) const {
        if (not open) {
            product_views = {world_view};
            return;
        }
        if (state.paletteMode) {
            if (not state.paletteScene.root or not state.paletteScene.camera) {
                product_views = {world_view};
                return;
            }
            product_views = {
                rmmr::wrapper::Product::View{
                    .viewport = world_view.viewport,
                    .scene = *state.paletteScene.root,
                    .camera = *state.paletteScene.camera,
                },
            };
            return;
        }
        if (not state.mainScene.root or not state.mainScene.camera) {
            product_views = {world_view};
            return;
        }
        product_views = {
            rmmr::wrapper::Product::View{
                .viewport = world_view.viewport,
                .scene = *state.mainScene.root,
                .camera = *state.mainScene.camera,
            },
        };
    }

}
