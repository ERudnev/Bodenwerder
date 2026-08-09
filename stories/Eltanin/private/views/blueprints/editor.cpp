#include "views/blueprints/editor.h"

#include "views/blueprints/geometry.h"

#include "mech/semantics/quarks.h"
#include "mech/semantics/shapes.h"
#include "mech/semantics/space.h"
#include "mech/semantics/subframe.h"

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
#include <format>
#include <numbers>
#include <set>
#include <tuple>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace eltanin::views {

    using namespace rmmr;
    using namespace rmmr::resource;
    using blueprints::geometry::QuarkActor;

    namespace {

        constexpr float cursorOpacity = 0.18f;
        // Cell indices: inclusive pen. +49 (not +50) so the last cell stays inside grid lines −50…+50.
        constexpr base::common_types::integer cursorLatticeMin = -50;
        constexpr base::common_types::integer cursorLatticeMax = 49;
        constexpr float gridOpacity = 0.88f;

        struct CellPosLess {
            auto operator()(const base::common_types::index3& a, const base::common_types::index3& b) const -> bool {
                return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
            }
        };

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

        auto sameCellPose(const mech::space::cell::Pose& a, const mech::space::cell::Pose& b) -> bool {
            return sameIndex3(a.pos, b.pos) and a.ori == b.ori;
        }

        auto findActorByAlias(Reading context, const std::vector<QuarkActor>& actors, renderer::Integer32 alias) -> base::maybe<QuarkActor> {
            for (const auto& actor : actors) {
                if (not with<scene::actor::Identified>::exists(context, actor.id))
                    continue;
                if (with<scene::actor::Identified>::get(context, actor.id).scenicAlias == alias)
                    return actor;
            }
            return {};
        }

        void eraseDescending(auto& vec, const std::set<std::size_t>& indices) {
            for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
                if (*it < vec.size())
                    vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(*it));
            }
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

        auto quarkPose(const mech::Blueprint& data, const QuarkActor& actor) -> base::maybe<mech::space::cell::Pose> {
            switch (actor.kind) {
                case QuarkActor::Kind::knot:
                    if (actor.index < data.knots.size())
                        return data.knots[actor.index].pose;
                    break;
                case QuarkActor::Kind::halfChord:
                    if (actor.index < data.halfChords.size())
                        return data.halfChords[actor.index].pose;
                    break;
            }
            return {};
        }

        auto selectionRowLabel(const mech::Blueprint* data, const base::maybe<QuarkActor>& actor, renderer::Integer32 alias) -> std::string {
            const auto hash = fqsm::internal::id::info_hash(static_cast<fqsm::internal::id::BaseType>(alias));
            if (not actor or not data)
                return std::format("?  #{}", hash);
            switch (actor->kind) {
                case QuarkActor::Kind::knot: {
                    if (actor->index >= data->knots.size())
                        return std::format("knot[{}]  #{}", actor->index, hash);
                    const auto& knot = data->knots[actor->index];
                    const auto& pos = knot.pose.pos;
                    return std::format("knot[{}] {}  cell[{},{},{}] ori={}  #{}", actor->index, mech::subframe::corner::specs.at(knot.kind).code, pos.x, pos.y, pos.z, knot.pose.ori, hash);
                }
                case QuarkActor::Kind::halfChord: {
                    if (actor->index >= data->halfChords.size())
                        return std::format("halfChord[{}]  #{}", actor->index, hash);
                    const auto& halfChord = data->halfChords[actor->index];
                    const auto& pos = halfChord.pose.pos;
                    const char pole = halfChord.pole == mech::subframe::halfEdge::Pole::s ? 's' : 'e';
                    return std::format("halfChord[{}] {}:{}  cell[{},{},{}] ori={}  #{}", actor->index, mech::subframe::halfEdge::specs.at(halfChord.kind).code, pole, pos.x, pos.y, pos.z, halfChord.pose.ori, hash);
                }
            }
            return std::format("?  #{}", hash);
        }

        auto selectionRefs(Reading context, const std::vector<QuarkActor>& actors, const std::vector<renderer::Integer32>& selection) -> std::vector<std::pair<QuarkActor::Kind, std::size_t>> {
            std::vector<std::pair<QuarkActor::Kind, std::size_t>> refs;
            for (const auto alias : selection) {
                if (const auto actor = findActorByAlias(context, actors, alias))
                    refs.emplace_back(actor->kind, actor->index);
            }
            return refs;
        }

        void restoreSelection(Reading context, const std::vector<QuarkActor>& actors, const std::vector<std::pair<QuarkActor::Kind, std::size_t>>& refs, std::vector<renderer::Integer32>& selection) {
            selection.clear();
            for (const auto& [kind, index] : refs) {
                for (const auto& actor : actors) {
                    if (actor.kind != kind or actor.index != index)
                        continue;
                    if (not with<scene::actor::Identified>::exists(context, actor.id))
                        break;
                    selection.push_back(with<scene::actor::Identified>::get(context, actor.id).scenicAlias);
                    break;
                }
            }
        }

        auto actorAlias(Reading context, const QuarkActor& actor) -> base::maybe<renderer::Integer32> {
            if (not with<scene::actor::Identified>::exists(context, actor.id))
                return {};
            return with<scene::actor::Identified>::get(context, actor.id).scenicAlias;
        }

        void addSelectionAlias(std::vector<renderer::Integer32>& selection, renderer::Integer32 alias) {
            if (alias == renderer::Integer32{0})
                return;
            if (std::find(selection.begin(), selection.end(), alias) == selection.end())
                selection.push_back(alias);
        }

        void removeSelectionAlias(std::vector<renderer::Integer32>& selection, renderer::Integer32 alias) {
            selection.erase(std::remove(selection.begin(), selection.end(), alias), selection.end());
        }

        // Cell family: all quark actors sharing pose.pos with the hit piece.
        auto familyAliases(Reading context, const std::vector<QuarkActor>& actors, const mech::Blueprint& data, const QuarkActor& hit) -> std::vector<renderer::Integer32> {
            const auto hitPose = quarkPose(data, hit);
            if (not hitPose)
                return {};
            std::vector<renderer::Integer32> aliases;
            for (const auto& actor : actors) {
                const auto pose = quarkPose(data, actor);
                if (not pose or not sameIndex3(pose->pos, hitPose->pos))
                    continue;
                if (const auto alias = actorAlias(context, actor))
                    aliases.push_back(*alias);
            }
            return aliases;
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

        state.scene = root;
        state.camera = camera;
        state.interframe = with<Assets>::find<meshpack::Asset>(context, Unit::Name::from("Eltanin", "interframe"));
        state.quarkActors = {};
        state.selection.clear();
        state.hovered.reset();
        state.spaceMenu = {.place = false, .close = false};
    }

    void Blueprints::show(Writing context, resource::blueprint::Asset::Id asset_id) {
        state.hovered = asset_id;
        state.selection.clear();
        syncVisuals(context);
    }

    void Blueprints::syncVisuals(Writing context) {
        if (not state.scene.exists() or not state.interframe.exists())
            return;
        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered)) {
            blueprints::geometry::clearActors(context, *state.scene, state.quarkActors);
            state.selection.clear();
            return;
        }
        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data;
        blueprints::geometry::syncActors(context, *state.scene, *state.interframe, data, state.quarkActors);
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

    void Blueprints::deleteSelection(Writing context) {
        if (state.selection.empty())
            return;
        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            return;

        std::set<std::size_t> knots;
        std::set<std::size_t> halfChords;
        for (const auto alias : state.selection) {
            const auto actor = findActorByAlias(context, state.quarkActors, alias);
            if (not actor)
                continue;
            switch (actor->kind) {
                case QuarkActor::Kind::knot: knots.insert(actor->index); break;
                case QuarkActor::Kind::halfChord: halfChords.insert(actor->index); break;
            }
        }

        state.selection.clear();
        if (knots.empty() and halfChords.empty())
            return;

        {
            auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
            eraseDescending(data->data.knots, knots);
            eraseDescending(data->data.halfChords, halfChords);
        }
        persistHovered(context);
        syncVisuals(context);
    }

    void Blueprints::rotateSelection(Writing context, mech::space::orient::Semiaxis axis) {
        if (state.selection.empty())
            return;
        if (not state.hovered.exists() or not with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
            return;

        // turn[] is local (right-multiply). Rigid cell band needs world left-multiply: compose[delta][ori].
        const auto delta = mech::space::orient::turn(axis)[0];
        const auto& composeRow = mech::space::orient::compose[static_cast<std::size_t>(delta)];

        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data;
        std::set<base::common_types::index3, CellPosLess> cells;
        for (const auto alias : state.selection) {
            const auto actor = findActorByAlias(context, state.quarkActors, alias);
            if (not actor)
                continue;
            if (const auto pose = quarkPose(data, *actor))
                cells.insert(pose->pos);
        }
        if (cells.empty())
            return;

        const auto refs = selectionRefs(context, state.quarkActors, state.selection);
        {
            auto writable = with<::eltanin::resource::blueprint::Asset>::modify(context, *state.hovered);
            for (auto& knot : writable->data.knots) {
                if (cells.contains(knot.pose.pos))
                    knot.pose.ori = composeRow[static_cast<std::size_t>(knot.pose.ori)];
            }
            for (auto& halfChord : writable->data.halfChords) {
                if (cells.contains(halfChord.pose.pos))
                    halfChord.pose.ori = composeRow[static_cast<std::size_t>(halfChord.pose.ori)];
            }
        }
        persistHovered(context);
        syncVisuals(context);
        restoreSelection(context, state.quarkActors, refs, state.selection);
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
        }

        updateWorldCursor(context);

        if (not ImGui::GetIO().WantCaptureMouse and under != renderer::Integer32{0}) {
            const auto hit = findActorByAlias(context, state.quarkActors, under);
            const bool shift = ImGui::GetIO().KeyShift;
            const mech::Blueprint* blueprintData = (state.hovered.exists() and with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
                ? &with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data
                : nullptr;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) and hit) {
                if (shift and blueprintData) {
                    for (const auto alias : familyAliases(context, state.quarkActors, *blueprintData, *hit))
                        addSelectionAlias(state.selection, alias);
                } else {
                    addSelectionAlias(state.selection, under);
                }
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) and hit) {
                if (shift and blueprintData) {
                    for (const auto alias : familyAliases(context, state.quarkActors, *blueprintData, *hit))
                        removeSelectionAlias(state.selection, alias);
                } else {
                    removeSelectionAlias(state.selection, under);
                }
            }
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) and not ImGui::GetIO().WantCaptureMouse and under == renderer::Integer32{0}) {
            state.selection.clear();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete) and not ImGui::GetIO().WantCaptureKeyboard)
            deleteSelection(context);

        if (not state.selection.empty() and not ImGui::GetIO().WantCaptureKeyboard) {
            using Semiaxis = mech::space::orient::Semiaxis;
            base::maybe<Semiaxis> axis;
            if (ImGui::IsKeyPressed(ImGuiKey_A)) axis = Semiaxis::Yn;
            else if (ImGui::IsKeyPressed(ImGuiKey_D)) axis = Semiaxis::Yp;
            else if (ImGui::IsKeyPressed(ImGuiKey_W)) axis = Semiaxis::Xp;
            else if (ImGui::IsKeyPressed(ImGuiKey_S)) axis = Semiaxis::Xn;
            else if (ImGui::IsKeyPressed(ImGuiKey_Q)) axis = Semiaxis::Zn;
            else if (ImGui::IsKeyPressed(ImGuiKey_E)) axis = Semiaxis::Zp;
            if (axis)
                rotateSelection(context, *axis);
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
                                if (existing.kind == knot.kind and sameCellPose(existing.pose, knot.pose)) {
                                    occupied = true;
                                    break;
                                }
                            }
                            if (not occupied)
                                data->data.knots.push_back(knot);
                        }
                        for (const auto& halfChord : mech::quarks::seedHalfChords(shape, cell)) {
                            bool occupied = false;
                            for (const auto& existing : data->data.halfChords) {
                                if (existing.kind == halfChord.kind and existing.pole == halfChord.pole and sameCellPose(existing.pose, halfChord.pose)) {
                                    occupied = true;
                                    break;
                                }
                            }
                            if (not occupied)
                                data->data.halfChords.push_back(halfChord);
                        }
                    });
                    if (applied) {
                        ImGui::CloseCurrentPopup();
                        state.spaceMenu = {.place = false, .close = false};
                        state.selection.clear();
                        persistHovered(context);
                        syncVisuals(context);
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
                ImGui::Text("Half-chords: %zu", data.halfChords.size());
                ImGui::Text("Actors: %zu", state.quarkActors.size());
                ImGui::Text("Selection: %zu", state.selection.size());
                ImGui::Text("Cursor [%d, %d, %d]", state.cursorLattice.x, state.cursorLattice.y, state.cursorLattice.z);
                ImGui::Text("Floor: %d", state.currentFloor);
                ImGui::TextDisabled("PgUp/PgDn · Space · LMB/+Shift family · RMB deselect · Del · WASD/QE");
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

        if (not state.selection.empty()) {
            ImGui::SetNextWindowPos(ImVec2{blueprintsPos.x + blueprintsSize.x + 8.0f, blueprintsPos.y}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2{360.0f, 220.0f}, ImGuiCond_FirstUseEver);
            bool selectionOpen = true;
            if (ImGui::Begin("Selection", &selectionOpen)) {
                ImGui::TextDisabled("Del — remove · x/RMB deselect · Shift+LMB/RMB — cell family");
                const auto* blueprintData = (state.hovered.exists() and with<::eltanin::resource::blueprint::Asset>::exists(context, *state.hovered))
                    ? &with<::eltanin::resource::blueprint::Asset>::get(context, *state.hovered).data
                    : nullptr;
                for (std::size_t index = 0; index < state.selection.size();) {
                    const auto alias = state.selection[index];
                    const auto actor = findActorByAlias(context, state.quarkActors, alias);
                    const auto row = selectionRowLabel(blueprintData, actor, alias);
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
