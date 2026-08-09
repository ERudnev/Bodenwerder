#include "views/blueprints/editor.h"

#include "views/blueprints/patterns.h"

#include "mech/semantics/levelOne.h"
#include "mech/semantics/shapes.h"
#include "mech/semantics/space.h"

#include <eltanin/resources/assets.q1.h>
#include <eltanin/world.q1.h>
#include <fQSM/identifier.h>
#include <rmmr/controller/cameraOrbit.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/window.q1.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <map>
#include <numbers>
#include <set>
#include <utility>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace eltanin::views {

    using namespace rmmr;
    using namespace rmmr::resource;

    namespace {

        constexpr float cursorOpacity = 0.18f;
        constexpr float cursorClampMeters = 40.0f;
        constexpr float gridOpacity = 0.88f;
        inline constexpr bool contentAutoSave = true;

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

        void syncCameraPivotToFloor(Writing context, Blueprints::State& state) {
            if (not state.camera.exists())
                return;
            const float cell = mech::physical::edgeMeters;
            const float floorY = static_cast<float>(state.currentFloor) * cell;
            auto orbit = with<controller::CameraOrbit>::modify(context, *state.camera);
            orbit->pivot.y = floorY;
            applyOrbitPose(context, *state.camera);
        }

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
            }
            return false;
        }

        auto layerLabel(mech::layer layer) -> const char* {
            switch (layer) {
                case mech::layer::plate: return "plate";
                case mech::layer::frame: return "frame";
                case mech::layer::inner: return "inner";
            }
            return "?";
        }

        auto floorPasses(Blueprints::FloorFilter filter, int floor, int currentFloor) -> bool {
            switch (filter) {
                case Blueprints::FloorFilter::all: return true;
                case Blueprints::FloorFilter::onlyCurrent: return floor == currentFloor;
                case Blueprints::FloorFilter::notAbove: return floor <= currentFloor;
            }
            return true;
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

        // cell.pose ⊕ local piece orient (both about cell center).
        auto pieceScenePose(const mech::Pose& cell, mech::orient::key local) -> Pose {
            const auto R = glm::mat3(mech::orient::matrix[static_cast<std::size_t>(cell.ori)]) * glm::mat3(mech::orient::matrix[static_cast<std::size_t>(local)]);
            const float edge = mech::physical::edgeMeters;
            return Pose{
                .position = Pos{
                    static_cast<float>(cell.pos.x) * edge,
                    static_cast<float>(cell.pos.y) * edge,
                    static_cast<float>(cell.pos.z) * edge,
                },
                .rotation = glm::normalize(glm::quat_cast(R)),
            };
        }

        auto latticeWorldPos(const base::common_types::index3& lattice) -> Pos {
            const float cell = mech::physical::edgeMeters;
            return Pos{
                static_cast<float>(lattice.x) * cell,
                static_cast<float>(lattice.y) * cell,
                static_cast<float>(lattice.z) * cell,
            };
        }

        auto actorLattice(Reading context, const Blueprints::State& state, const Blueprints::Actor& actor) -> base::maybe<base::common_types::index3> {
            if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
                return {};
            const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data;
            switch (actor.source) {
                case Blueprints::Source::inner:
                case Blueprints::Source::corner:
                case Blueprints::Source::halfEdge:
                    if (actor.index < data.cells.size())
                        return data.cells[actor.index].pose.pos;
                    break;
                case Blueprints::Source::plate:
                    if (actor.index < data.hull.size())
                        return data.hull[actor.index].pose.pos;
                    break;
            }
            return {};
        }

        auto sourceLabel(Blueprints::Source source) -> const char* {
            switch (source) {
                case Blueprints::Source::plate: return "plate";
                case Blueprints::Source::inner: return "inner";
                case Blueprints::Source::corner: return "corner";
                case Blueprints::Source::halfEdge: return "halfEdge";
            }
            return "?";
        }

        auto isCellSource(Blueprints::Source source) -> bool {
            return source == Blueprints::Source::inner or source == Blueprints::Source::corner or source == Blueprints::Source::halfEdge;
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

        template<typename Fn>
        auto frameShapePicks(base::maybe<mech::frame::shape> current, Fn&& onPick) -> bool {
            bool applied = false;
            const auto pick = [&](const char* label, mech::frame::shape value) {
                const bool hit = current ? shapeItem(label, *current, value) : ImGui::Selectable(label);
                if (hit) {
                    onPick(value);
                    applied = true;
                }
            };
            ImGui::TextUnformatted("Frame shape");
            ImGui::Separator();
            pick("k8", mech::frame::shape::k8);
            pick("k7", mech::frame::shape::k7);
            pick("k6", mech::frame::shape::k6);
            pick("k4", mech::frame::shape::k4);
            pick("k4f1111", mech::frame::shape::k4f1111);
            pick("k3f121", mech::frame::shape::k3f121);
            pick("k4f2121", mech::frame::shape::k4f2121);
            pick("k3f222", mech::frame::shape::k3f222);
            return applied;
        }

        auto firstViewport(Reading context) -> base::maybe<system::Viewport::Id> {
            for (const auto [id, _] : context->aspect<system::Viewport>().items())
                return id;
            return {};
        }

        auto firstWindow(Reading context) -> base::maybe<system::Window::Id> {
            for (const auto [id, _] : context->aspect<system::Window>().items())
                return id;
            return {};
        }

        auto rayHitFloor(Reading context, scene::Camera::Id camera, system::Viewport::Id viewport, index2 mouse, float planeY) -> base::maybe<Pos> {
            const auto& vp = with<system::Viewport>::get(context, viewport);
            if (vp.size.x <= 0 or vp.size.y <= 0)
                return {};
            const float localX = static_cast<float>(mouse.x - vp.origin.x);
            const float localY = static_cast<float>(mouse.y - vp.origin.y);
            if (localX < 0.0f or localY < 0.0f or localX >= static_cast<float>(vp.size.x) or localY >= static_cast<float>(vp.size.y))
                return {};

            const float ndcX = (2.0f * localX / static_cast<float>(vp.size.x)) - 1.0f;
            const float ndcY = 1.0f - (2.0f * localY / static_cast<float>(vp.size.y));
            const float aspect = static_cast<float>(vp.size.x) / static_cast<float>(vp.size.y);
            const mat4 inv = glm::inverse(scene::Camera::Actions::view_projection(context, camera, aspect));
            auto unproject = [&](float ndcZ) -> Pos {
                const vec4 clip{ndcX, ndcY, ndcZ, 1.0f};
                const vec4 world = inv * clip;
                return Pos{world.x, world.y, world.z} / world.w;
            };
            const Pos near = unproject(-1.0f);
            const Pos far = unproject(1.0f);
            const Pos dir = far - near;
            if (std::abs(dir.y) < 1.0e-6f)
                return {};
            const float t = (planeY - near.y) / dir.y;
            if (t < 0.0f)
                return {};
            return near + dir * t;
        }

    } // namespace

    void Blueprints::create(Writing context, filepath directory) {
        const auto grid_name = Unit::Name::from("rmmr", "grid");
        const auto grid_geometry = with<Assets>::find<geometry::Asset>(context, grid_name);
        const auto grid_material = with<Assets>::find<rmmr::resource::material::Asset>(context, grid_name);
        if (not grid_geometry or not grid_material)
            return (void)context.refuse("eltanin::views::Blueprints::create: rmmr::grid geometry/material missing");

        const auto kube_geometry = with<Assets>::find<geometry::Asset>(context, Unit::Name::from("rmmr", "kube"));
        const auto cursor_material = with<Assets>::find<rmmr::resource::material::Asset>(context, Unit::Name::from("Eltanin", "type"));
        if (not kube_geometry or not cursor_material)
            return (void)context.refuse("eltanin::views::Blueprints::create: kube / lit-transparent material missing");
        const auto device = with<World>::get_global(context).window;
        if (not device)
            return (void)context.refuse("eltanin::views::Blueprints::create: World window missing");

        const auto root = with<scene::Interface>::createScene(context);

        const float cell = mech::physical::edgeMeters;
        const float patternScale = 1.0f / cell;
        state.currentFloor = 0;
        state.floorFilter = FloorFilter::all;
        state.cursorLattice = base::common_types::index3{.x = 0, .y = 0, .z = 0};
        state.grid = with<scene::Interface>::createGrid(
            context,
            root,
            *device,
            Pose::from(Pos{-2.0f, -2.0f, -2.0f}, HPB{0.0f, 0.0f, 0.0f}),
            item<scene::Grid>{.geometry = *grid_geometry, .material = *grid_material, .opacity = gridOpacity, .patternScale = patternScale});

        const float edge = mech::physical::edgeMeters;
        const auto cursorResolved = meshpack::Asset::Resolved{
            .geometry = *kube_geometry,
            .entry = geometry::EntryId{0},
            .surfaces = {{geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = *cursor_material, .textures = {}}}},
            .texpack = {},
        };
        const auto identityPose = renderer::DiscretePose{.pos = index3{0, 0, 0}, .ori = renderer::Signed32{0}};
        const auto cursorMesh = scene::actor::Mesh::Actions::compose(context, *device, {scene::actor::Mesh::Occurrence{.entry = cursorResolved, .pose = identityPose}});
        if (not cursorMesh)
            return (void)context.refuse("eltanin::views::Blueprints::create: cursor mesh composition failed");
        state.worldCursor = with<scene::Interface>::createMeshActor(context, root, Pose::from(latticeWorldPos(state.cursorLattice), HPB{0.0f, 0.0f, 0.0f}), std::move(*cursorMesh), scene::actor::MeshState::Quantum{
            .albedo = RGB{0.35f, 0.95f, 1.0f},
            .scale = vec3{edge, edge, edge},
            .latticeStep = 1.0f,
            .patternScale = 1.0f,
            .opacity = cursorOpacity,
            .visible = true,
        });

        const Pos pivot{0.0f, 0.0f, 0.0f};
        const Pos camera_pos{24.0f, 20.0f, 40.0f};
        const auto camera = with<scene::Interface>::createCamera(
            context,
            root,
            Pose::from(camera_pos, HPB{36.87f, -29.74f, 0.0f}),
            60.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::CameraOrbit>::create(context, camera, pivot, glm::length(camera_pos - pivot));

        syncCameraPivotToFloor(context, state);

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
        state.layers = Layers{.plate = false, .frame = true, .inner = false};
        state.floors.clear();
        state.levelOne.clear();
        state.levelTwo.clear();
        state.spaceMenu = {.target = {}, .place = false, .close = false};

        if (not std::filesystem::is_directory(directory))
            return (void)context.refuse(std::format("eltanin::views::Blueprints::create: not a directory '{}'", directory.string()));

        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (not entry.is_regular_file() or entry.path().extension() != ".blueprint")
                continue;

            const auto stem = entry.path().stem().string();
            const auto relative = filename{(std::filesystem::path{"blueprints"} / entry.path().filename()).generic_string()};
            const auto asset_id = with<::eltanin::resource::Assets>::add_blueprint_loader(
                context,
                Unit::Name::from("Eltanin", stem),
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
        syncVisuals(context);
    }

    void Blueprints::clearVisuals(Writing context) {
        for (const auto& actor : state.levelOne)
            destroyMeshActor(context, actor.id);
        for (const auto& actor : state.levelTwo)
            destroyMeshActor(context, actor.id);
        state.levelOne.clear();
        state.levelTwo.clear();
        state.floors.clear();
        state.selection.clear();
    }

    void Blueprints::syncVisuals(Writing context) {
        clearVisuals(context);

        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            return;

        // Fill from k* when legacy .blueprint has no subframe lists — not after user cleared (subframeBare).
        {
            auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
            for (auto& cell : data->data.cells) {
                if (cell.corners.empty() and cell.edges.empty() and not cell.subframeBare)
                    blueprints::patterns::apply(cell);
            }
        }

        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data;
        const auto pack_one = *with<Assets>::find<meshpack::Asset>(context, Unit::Name::from("Eltanin", "levelOne"));
        const auto pack_frame = with<Assets>::find<meshpack::Asset>(context, Unit::Name::from("Eltanin", "interframe"));

        for (std::size_t i = 0; i < data.cells.size(); ++i) {
            const auto& cell = data.cells[i];
            const int floor = static_cast<int>(cell.pose.pos.y);
            if (pack_frame) {
                for (std::size_t c = 0; c < cell.corners.size(); ++c) {
                    const auto& piece = cell.corners[c];
                    const auto entry = std::string{blueprints::patterns::cornerMesh(piece.kind)};
                    if (auto actor = spawnFromPack(context, *pack_frame, entry, pieceScenePose(cell.pose, piece.orient), mech::layer::frame, Source::corner, i, c, mech::subframe::halfEdge::Pole::s, floor, RGB{1.0f, 1.0f, 1.0f}, 1.0f))
                        state.levelOne.push_back(*actor);
                }
                for (std::size_t e = 0; e < cell.edges.size(); ++e) {
                    const auto& edge = cell.edges[e];
                    const auto pose = pieceScenePose(cell.pose, edge.orient);
                    const auto poleAtMesh0 = edge.poleAtMesh0;
                    const auto poleAtMeshRay = mech::subframe::halfEdge::opposite(edge.poleAtMesh0);
                    if (auto mesh0 = spawnFromPack(context, *pack_frame, blueprints::patterns::halfEdgeMesh(edge.kind, poleAtMesh0), pose, mech::layer::frame, Source::halfEdge, i, e, poleAtMesh0, floor, RGB{1.0f, 1.0f, 1.0f}, 1.0f))
                        state.levelOne.push_back(*mesh0);
                    if (auto meshRay = spawnFromPack(context, *pack_frame, blueprints::patterns::halfEdgeMesh(edge.kind, poleAtMeshRay), pose, mech::layer::frame, Source::halfEdge, i, e, poleAtMeshRay, floor, RGB{1.0f, 1.0f, 1.0f}, 1.0f))
                        state.levelOne.push_back(*meshRay);
                }
            }
            if (auto inner = spawnFromPack(context, pack_one, mech::levelOne::innerMesh(cell.shape), scenePose(cell.pose), mech::layer::inner, Source::inner, i, 0, mech::subframe::halfEdge::Pole::s, floor, mech::slot::color(cell.role), cell.shape == mech::frame::shape::k8 ? 1.0f : 0.45f))
                state.levelOne.push_back(*inner);
        }
        for (std::size_t i = 0; i < data.hull.size(); ++i) {
            const auto& plate = data.hull[i];
            if (auto a = spawnFromPack(context, pack_one, mech::levelOne::mesh(plate.shape), scenePose(plate.pose), mech::layer::plate, Source::plate, i, 0, mech::subframe::halfEdge::Pole::s, static_cast<int>(plate.pose.pos.y), RGB{1.0f, 1.0f, 1.0f}, 1.0f))
                state.levelOne.push_back(*a);
        }

        for (const auto& actor : state.levelOne)
            state.floors[actor.floor].push_back(actor.id);
        for (const auto& actor : state.levelTwo)
            state.floors[actor.floor].push_back(actor.id);

        applyLayers(context);
    }

    void Blueprints::deleteSelection(Writing context) {
        if (state.selection.empty())
            return;
        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            return;

        std::set<std::size_t> killCells;
        std::set<std::size_t> killPlates;
        std::map<std::size_t, std::set<std::size_t>> killCorners;
        std::map<std::size_t, std::set<std::size_t>> killEdges;
        for (const auto alias : state.selection) {
            auto actor = findActorByAlias(context, state.levelOne, alias);
            if (not actor)
                actor = findActorByAlias(context, state.levelTwo, alias);
            if (not actor)
                continue;
            switch (actor->source) {
                case Source::inner: killCells.insert(actor->index); break;
                case Source::plate: killPlates.insert(actor->index); break;
                case Source::corner: killCorners[actor->index].insert(actor->sub); break;
                case Source::halfEdge: killEdges[actor->index].insert(actor->sub); break;
            }
        }

        state.selection.clear();
        if (killCells.empty() and killPlates.empty() and killCorners.empty() and killEdges.empty())
            return;

        {
            auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
            for (const auto& [cellIndex, subs] : killCorners) {
                if (cellIndex >= data->data.cells.size())
                    continue;
                auto& cell = data->data.cells[cellIndex];
                eraseDescending(cell.corners, subs);
                cell.subframeBare = true;
            }
            for (const auto& [cellIndex, subs] : killEdges) {
                if (cellIndex >= data->data.cells.size())
                    continue;
                auto& cell = data->data.cells[cellIndex];
                eraseDescending(cell.edges, subs);
                cell.subframeBare = true;
            }
            eraseDescending(data->data.cells, killCells);
            eraseDescending(data->data.hull, killPlates);
        }
        persistHovered(context);
        syncVisuals(context);
    }

    void Blueprints::rotateSelection(Writing context, const std::vector<mech::orient::key>& turn) {
        if (state.selection.empty() or turn.size() < 24)
            return;
        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            return;

        std::set<std::size_t> cells;
        std::set<std::size_t> plates;
        for (const auto alias : state.selection) {
            auto actor = findActorByAlias(context, state.levelOne, alias);
            if (not actor)
                actor = findActorByAlias(context, state.levelTwo, alias);
            if (not actor)
                continue;
            switch (actor->source) {
                case Source::inner:
                case Source::corner:
                case Source::halfEdge: cells.insert(actor->index); break;
                case Source::plate: plates.insert(actor->index); break;
            }
        }
        if (cells.empty() and plates.empty())
            return;

        {
            auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
            for (const auto index : cells) {
                if (index >= data->data.cells.size())
                    continue;
                auto& ori = data->data.cells[index].pose.ori;
                ori = turn[static_cast<std::size_t>(ori)];
            }
            for (const auto index : plates) {
                if (index >= data->data.hull.size())
                    continue;
                auto& ori = data->data.hull[index].pose.ori;
                ori = turn[static_cast<std::size_t>(ori)];
            }
        }

        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data;
        const auto refresh = [&](const Actor& actor) {
            if (not with<scene::Node>::exists(context, actor.id))
                return;
            switch (actor.source) {
                case Source::plate:
                    if (plates.contains(actor.index) and actor.index < data.hull.size())
                        with<scene::Node>::modify(context, actor.id)->pose = scenePose(data.hull[actor.index].pose);
                    break;
                case Source::inner:
                    if (cells.contains(actor.index) and actor.index < data.cells.size())
                        with<scene::Node>::modify(context, actor.id)->pose = scenePose(data.cells[actor.index].pose);
                    break;
                case Source::corner:
                    if (cells.contains(actor.index) and actor.index < data.cells.size()) {
                        const auto& cell = data.cells[actor.index];
                        if (actor.sub < cell.corners.size())
                            with<scene::Node>::modify(context, actor.id)->pose = pieceScenePose(cell.pose, cell.corners[actor.sub].orient);
                    }
                    break;
                case Source::halfEdge:
                    if (cells.contains(actor.index) and actor.index < data.cells.size()) {
                        const auto& cell = data.cells[actor.index];
                        if (actor.sub < cell.edges.size())
                            with<scene::Node>::modify(context, actor.id)->pose = pieceScenePose(cell.pose, cell.edges[actor.sub].orient);
                    }
                    break;
            }
        };
        for (const auto& actor : state.levelOne)
            refresh(actor);
        for (const auto& actor : state.levelTwo)
            refresh(actor);

        persistHovered(context);
    }

    void Blueprints::persistHovered(Writing context) {
        if (not contentAutoSave)
            return;
        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            return;
        if (not with<::eltanin::resource::blueprint::Loader>::exists(context, *state.hovered))
            return;
        ::eltanin::resource::blueprint::Loader::Actions::save(context, *state.hovered);
    }

    auto Blueprints::spawnFromPack(Writing context, meshpack::Asset::Id pack, const std::string& entry, const Pose& pose, mech::layer layer, Source source, std::size_t index, std::size_t sub, mech::subframe::halfEdge::Pole pole, int floor, RGB albedo, float opacity) -> base::maybe<Actor> {
        if (entry.empty())
            return {};
        const auto resolved = meshpack::Asset::Actions::resolve(context, pack, entry);
        if (not resolved)
            return {};
        const auto device = with<World>::get_global(context).window;
        if (not device)
            return {};
        const auto identityPose = renderer::DiscretePose{.pos = index3{0, 0, 0}, .ori = renderer::Signed32{0}};
        const auto mesh = scene::actor::Mesh::Actions::compose(context, *device, {scene::actor::Mesh::Occurrence{.entry = *resolved, .pose = identityPose}});
        if (not mesh)
            return {};
        const auto id = with<scene::Interface>::createMeshActor(context, *state.scene, pose, std::move(*mesh), scene::actor::MeshState::Quantum{
            .albedo = albedo,
            .scale = vec3{1.0f, 1.0f, 1.0f},
            .latticeStep = 1.0f,
            .patternScale = 1.0f,
            .opacity = opacity,
            .visible = true,
        });
        scene::actor::Identified::Actions::extend(context, id);
        return Actor{.id = id, .layer = layer, .source = source, .index = index, .sub = sub, .pole = pole, .floor = floor};
    }

    void Blueprints::applyLayers(Writing context) {
        for (const auto& actor : state.levelOne)
            scene::actor::MeshState::Actions::setVisible(context, actor.id, layerVisible(state.layers, actor.layer) and floorPasses(state.floorFilter, actor.floor, state.currentFloor));
        for (const auto& actor : state.levelTwo)
            scene::actor::MeshState::Actions::setVisible(context, actor.id, layerVisible(state.layers, actor.layer) and floorPasses(state.floorFilter, actor.floor, state.currentFloor));
    }

    void Blueprints::syncGridToFloor(Writing context) {
        if (not state.grid.exists() or not with<scene::Node>::exists(context, *state.grid))
            return;
        const float cell = mech::physical::edgeMeters;
        const float y = static_cast<float>(state.currentFloor) * cell - 2.0f;
        with<scene::Node>::modify(context, *state.grid)->pose = Pose::from(Pos{-2.0f, y, -2.0f}, HPB{0.0f, 0.0f, 0.0f});
        syncCameraPivotToFloor(context, state);
    }

    void Blueprints::updateWorldCursor(Writing context, renderer::Integer32 under) {
        if (not state.worldCursor.exists() or not with<scene::Node>::exists(context, *state.worldCursor))
            return;

        base::maybe<base::common_types::index3> lattice;
        if (under != renderer::Integer32{0}) {
            auto actor = findActorByAlias(context, state.levelOne, under);
            if (not actor)
                actor = findActorByAlias(context, state.levelTwo, under);
            if (actor)
                lattice = actorLattice(context, state, *actor);
        }

        if (not lattice) {
            const float cell = mech::physical::edgeMeters;
            const float planeY = static_cast<float>(state.currentFloor) * cell;
            lattice = base::common_types::index3{.x = state.cursorLattice.x, .y = state.currentFloor, .z = state.cursorLattice.z};
            if (state.camera.exists()) {
                if (const auto window = firstWindow(context); window and not ImGui::GetIO().WantCaptureMouse) {
                    if (const auto viewport = firstViewport(context)) {
                        const auto mouse = with<system::Window>::get(context, *window).current.mouse;
                        if (const auto hit = rayHitFloor(context, *state.camera, *viewport, mouse, planeY)) {
                            const float x = std::clamp(hit->x, -cursorClampMeters, cursorClampMeters);
                            const float z = std::clamp(hit->z, -cursorClampMeters, cursorClampMeters);
                            lattice = base::common_types::index3{
                                .x = static_cast<base::common_types::integer>(std::lround(x / cell)),
                                .y = state.currentFloor,
                                .z = static_cast<base::common_types::integer>(std::lround(z / cell)),
                            };
                        }
                    }
                }
            }
            // Ray miss / far / UI capture: keep XZ, force current floor height.
            lattice->y = state.currentFloor;
            lattice->x = std::clamp(lattice->x, static_cast<base::common_types::integer>(std::lround(-cursorClampMeters / cell)), static_cast<base::common_types::integer>(std::lround(cursorClampMeters / cell)));
            lattice->z = std::clamp(lattice->z, static_cast<base::common_types::integer>(std::lround(-cursorClampMeters / cell)), static_cast<base::common_types::integer>(std::lround(cursorClampMeters / cell)));
        }

        state.cursorLattice = *lattice;
        with<scene::Node>::modify(context, *state.worldCursor)->pose = Pose::from(latticeWorldPos(state.cursorLattice), HPB{0.0f, 0.0f, 0.0f});
    }

    void Blueprints::draw(Writing context, bool& open) {
        if (not open) {
            if (not state.levelOne.empty() or not state.levelTwo.empty())
                clearVisuals(context);
            return;
        }

        if (state.levelOne.empty() and state.levelTwo.empty() and state.hovered.exists() and with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            syncVisuals(context);

        renderer::Integer32 under = renderer::Integer32{0};
        for (const auto [_, window] : context->aspect<system::Window>().items()) {
            under = window.current.under;
            break;
        }

        if (not ImGui::GetIO().WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
                ++state.currentFloor;
                syncGridToFloor(context);
                applyLayers(context);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
                --state.currentFloor;
                syncGridToFloor(context);
                applyLayers(context);
            }
        }

        updateWorldCursor(context, under);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) and not ImGui::GetIO().WantCaptureMouse and under != renderer::Integer32{0}) {
            const bool known = findActorByAlias(context, state.levelOne, under).exists() or findActorByAlias(context, state.levelTwo, under).exists();
            if (known and std::find(state.selection.begin(), state.selection.end(), under) == state.selection.end())
                state.selection.push_back(under);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete) and not ImGui::GetIO().WantCaptureKeyboard)
            deleteSelection(context);

        if (not state.selection.empty() and not ImGui::GetIO().WantCaptureKeyboard) {
            const std::vector<mech::orient::key>* turn = nullptr;
            if (ImGui::IsKeyPressed(ImGuiKey_A)) turn = &mech::orient::turnYn;
            else if (ImGui::IsKeyPressed(ImGuiKey_D)) turn = &mech::orient::turnYp;
            else if (ImGui::IsKeyPressed(ImGuiKey_W)) turn = &mech::orient::turnXp;
            else if (ImGui::IsKeyPressed(ImGuiKey_S)) turn = &mech::orient::turnXn;
            else if (ImGui::IsKeyPressed(ImGuiKey_Q)) turn = &mech::orient::turnZn;
            else if (ImGui::IsKeyPressed(ImGuiKey_E)) turn = &mech::orient::turnZp;
            if (turn)
                rotateSelection(context, *turn);
        }

        constexpr auto spaceMenuPopup = "##blueprints.spaceMenu";
        const bool spaceMenuOpen = ImGui::IsPopupOpen(spaceMenuPopup);
        const bool spaceMenuActive = state.spaceMenu.target.exists() or state.spaceMenu.place;
        if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
            if (spaceMenuOpen or spaceMenuActive) {
                state.spaceMenu.close = true;
            } else if (not ImGui::GetIO().WantCaptureKeyboard) {
                if (under != renderer::Integer32{0}) {
                    auto actor = findActorByAlias(context, state.levelOne, under);
                    if (not actor)
                        actor = findActorByAlias(context, state.levelTwo, under);
                    if (actor) {
                        state.spaceMenu = {.target = *actor, .place = false, .close = false};
                        ImGui::OpenPopup(spaceMenuPopup);
                        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
                    }
                } else {
                    state.spaceMenu = {.target = {}, .place = true, .close = false};
                    ImGui::OpenPopup(spaceMenuPopup);
                    ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
                }
            }
        }

        if (spaceMenuActive) {
            if (ImGui::BeginPopup(spaceMenuPopup)) {
                if (state.spaceMenu.close) {
                    ImGui::CloseCurrentPopup();
                    state.spaceMenu = {.target = {}, .place = false, .close = false};
                } else {
                    bool applied = false;
                    if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered)) {
                        ImGui::TextDisabled("No blueprint");
                    } else if (state.spaceMenu.place) {
                        auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
                        applied = frameShapePicks({}, [&](mech::frame::shape shape) {
                            mech::Element::Cell cell{
                                .pose = mech::Pose{.pos = state.cursorLattice, .ori = 0},
                                .shape = shape,
                                .role = mech::slot::inner::multi,
                                .corners = {},
                                .edges = {},
                                .subframeBare = false,
                            };
                            blueprints::patterns::apply(cell);
                            data->data.cells.push_back(std::move(cell));
                        });
                    } else {
                        const Actor target = *state.spaceMenu.target;
                        auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
                        if (isCellSource(target.source) and target.index < data->data.cells.size()) {
                            auto& cell = data->data.cells[target.index];
                            applied = frameShapePicks(cell.shape, [&](mech::frame::shape next) {
                                cell.shape = next;
                                cell.subframeBare = false;
                                blueprints::patterns::apply(cell);
                            });
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
                        state.spaceMenu = {.target = {}, .place = false, .close = false};
                        state.selection.clear();
                        persistHovered(context);
                        syncVisuals(context);
                    }
                }
                ImGui::EndPopup();
            } else {
                state.spaceMenu = {.target = {}, .place = false, .close = false};
            }
        }

        bool shown = open;
        ImVec2 blueprintsPos{};
        ImVec2 blueprintsSize{};
        if (ImGui::Begin("Blueprints", &shown)) {
            blueprintsPos = ImGui::GetWindowPos();
            blueprintsSize = ImGui::GetWindowSize();
            ImGui::TextDisabled(contentAutoSave ? "Content edits save .blueprint immediately" : "Content edits in memory only (auto-save off)");
            ImGui::BeginChild("blueprintList", ImVec2{220.0f, 0.0f}, true);
            if (state.loaded.empty()) {
                ImGui::TextDisabled("No blueprints loaded.");
            } else {
                for (const auto asset_id : state.loaded) {
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
                ImGui::Text("Unit: %s", unit.name.text().c_str());
                ImGui::Text("Name: %s", data.name.c_str());
                ImGui::Text("Author: %s", data.author.c_str());
                ImGui::Separator();
                ImGui::Text("Cells: %zu", data.cells.size());
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
                ImGui::Text("World cursor [%d, %d, %d]", state.cursorLattice.x, state.cursorLattice.y, state.cursorLattice.z);
                ImGui::Separator();
                ImGui::TextUnformatted("Layers (levelOne)");
                bool visibilityChanged = false;
                visibilityChanged |= ImGui::Checkbox("plate", &state.layers.plate);
                visibilityChanged |= ImGui::Checkbox("frame", &state.layers.frame);
                visibilityChanged |= ImGui::Checkbox("inner", &state.layers.inner);
                ImGui::Separator();
                ImGui::Text("Current floor: %d", state.currentFloor);
                ImGui::TextDisabled("PgUp / PgDn");
                ImGui::TextUnformatted("Floor filter");
                int filter = static_cast<int>(state.floorFilter);
                visibilityChanged |= ImGui::RadioButton("All floors", &filter, static_cast<int>(FloorFilter::all));
                visibilityChanged |= ImGui::RadioButton("Only current", &filter, static_cast<int>(FloorFilter::onlyCurrent));
                visibilityChanged |= ImGui::RadioButton("Not above current", &filter, static_cast<int>(FloorFilter::notAbove));
                state.floorFilter = static_cast<FloorFilter>(filter);
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
                ImGui::TextDisabled(contentAutoSave ? "Del — remove from model and save" : "Del — remove from model (memory only)");
                for (std::size_t index = 0; index < state.selection.size();) {
                    const auto alias = state.selection[index];
                    auto actor = findActorByAlias(context, state.levelOne, alias);
                    if (not actor)
                        actor = findActorByAlias(context, state.levelTwo, alias);
                    const auto hash = fqsm::internal::id::info_hash(static_cast<fqsm::internal::id::BaseType>(alias));
                    const auto row = actor
                        ? (actor->source == Source::halfEdge
                            ? std::format("{} {}[{}].edge{}:{}  #{}", layerLabel(actor->layer), sourceLabel(actor->source), actor->index, actor->sub, actor->pole == mech::subframe::halfEdge::Pole::s ? 's' : 'e', hash)
                            : actor->source == Source::corner
                                ? std::format("{} {}[{}].corner{}  #{}", layerLabel(actor->layer), sourceLabel(actor->source), actor->index, actor->sub, hash)
                                : std::format("{} {}[{}]  #{}", layerLabel(actor->layer), sourceLabel(actor->source), actor->index, hash))
                        : std::format("?  #{}", hash);
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
