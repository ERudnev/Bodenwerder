#include "views/blueprints/geometry.h"

#include "mech/semantics/role.h"
#include "mech/semantics/shapes.h"
#include "mech/semantics/subframe.h"
#include "views/blueprints/mountBounds.h"

#include <eltanin/mech/mount.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <cstddef>

#include <base/logging.h>

#include <format>
#include <string>

#include <glm/gtc/quaternion.hpp>

namespace eltanin::views::blueprints::geometry {

    using namespace rmmr;
    using namespace rmmr::resource;

    namespace {

        auto cornerMesh(mech::skeleton::Corner::Kind kind) -> std::string {
            return std::string{mech::skeleton::cornerSpecs.at(kind).code};
        }

        auto halfEdgeMesh(mech::skeleton::Halfrib::Kind kind, mech::skeleton::Halfrib::Pole pole) -> std::string {
            const char poleTag = pole == mech::skeleton::Halfrib::Pole::starts ? 's' : 'e';
            return std::format("{}{}", mech::skeleton::halfribSpecs.at(kind).code, poleTag);
        }

        auto membraneMesh(mech::skeleton::Membrane::Kind kind) -> std::string {
            return std::string{mech::skeleton::membraneSpecs.at(kind).code};
        }

        auto destroyActor(Writing context, scene::Root::Id root, scene::actor::Mesh::Id actor) -> void {
            if (with<scene::Node_group>::exists(context, root) and with<scene::Node_group>::get(context, root).contains(actor)) {
                with<scene::Node_group>::deleteElement(context, root, actor);
                return;
            }
            for (const auto [otherRoot, group] : context->aspect<scene::Node_group>().items()) {
                if (group.contains(actor)) {
                    with<scene::Node_group>::deleteElement(context, otherRoot, actor);
                    return;
                }
            }
            if (with<scene::Node>::exists(context, actor))
                with<scene::Node>::remove(context, actor);
        }

        auto entryOrigin(Reading context, const meshpack::Asset::Resolved& resolved) -> base::maybe<Pos> {
            if (not with<::rmmr::resource::geometry::Asset>::exists(context, resolved.geometry))
                return {};
            const auto& asset = with<::rmmr::resource::geometry::Asset>::get(context, resolved.geometry);
            if (resolved.entry >= asset.entries.size())
                return {};
            return asset.entries[resolved.entry].origin;
        }

        auto spawnIdentified(Writing context, scene::Root::Id root, Pose pose, const meshpack::Asset::Resolved& resolved) -> base::maybe<scene::actor::Mesh::Id> {
            const auto id = with<scene::Interface>::createMeshActor(context, root, pose, resolved);
            if (not with<scene::actor::Mesh>::exists(context, id))
                return {};
            with<scene::actor::Identified>::extend(context, id);
            return id;
        }

        auto spawnIdentified(Writing context, scene::Root::Id root, Pose pose, const meshpack::Asset::Resolved& resolved, RGB albedo) -> base::maybe<scene::actor::Mesh::Id> {
            const auto id = with<scene::Interface>::createMeshActor(context, root, pose, resolved, with<scene::actor::MeshState>::defaults(albedo, 1.0f));
            if (not with<scene::actor::Mesh>::exists(context, id))
                return {};
            with<scene::actor::Identified>::extend(context, id);
            return id;
        }

        auto mountAlbedo(const mech::Mount::Quantum& mount) -> RGB {
            if (mount.role.has_value())
                return mech::settings::colorCode(*mount.role);
            return RGB{1.0f, 1.0f, 1.0f};
        }

        auto spawnGhost(Writing context, scene::Root::Id root, Pose pose, meshpack::Asset::Resolved resolved, ::rmmr::resource::material::Asset::Id ghostMaterial, RGB albedo, float opacity) -> base::maybe<scene::actor::Mesh::Id> {
            for (auto& [_, surface] : resolved.surfaces)
                surface = ::rmmr::resource::material::Instance{.material = ghostMaterial, .textures = {}};
            const auto id = with<scene::Interface>::createMeshActor(context, root, pose, std::move(resolved), with<scene::actor::MeshState>::defaults(albedo, opacity));
            if (not with<scene::actor::Mesh>::exists(context, id))
                return {};
            return id;
        }

        auto resolveTempPart(Reading context, const mech::Mount::TempMesh& part) -> base::maybe<meshpack::Asset::Resolved> {
            const auto packId = with<::rmmr::resource::Assets>::find<meshpack::Asset>(context, part.pack);
            if (not packId)
                return {};
            return with<meshpack::Asset>::resolve(context, *packId, part.entry);
        }

        auto mountPartPose(Pose base, Pos anchorOrigin, Pos partOrigin) -> Pose {
            return Pose{.position = base.position + base.rotation * (partOrigin - anchorOrigin), .rotation = base.rotation};
        }

        auto partOrigin(Reading context, const meshpack::Asset::Resolved& resolved) -> Pos {
            if (const auto origin = entryOrigin(context, resolved))
                return *origin;
            return Pos{0.0f, 0.0f, 0.0f};
        }

        template <typename Spawn>
        auto spawnMountVisual(Writing context, Pose base, const mech::Mount::Quantum& mount, Spawn&& spawnOne) -> base::maybe<MountVisual> {
            if (mount.tempMesh.empty())
                return {};
            const auto firstResolved = resolveTempPart(context, mount.tempMesh.front());
            if (not firstResolved)
                return {};
            const auto firstId = spawnOne(base, *firstResolved);
            if (not firstId)
                return {};
            MountVisual visual{.id = *firstId, .extras = {}};
            const auto anchor = partOrigin(context, *firstResolved);
            for (std::size_t i = 1; i < mount.tempMesh.size(); ++i) {
                const auto resolved = resolveTempPart(context, mount.tempMesh[i]);
                if (not resolved)
                    continue;
                if (const auto extra = spawnOne(mountPartPose(base, anchor, partOrigin(context, *resolved)), *resolved))
                    visual.extras.push_back(*extra);
            }
            return visual;
        }

        void poseMountVisual(Writing context, const MountVisual& visual, const mech::Mount::Quantum& mount, Pose base) {
            if (with<scene::Node>::exists(context, visual.id))
                with<scene::Node>::modify(context, visual.id)->pose = base;
            if (mount.tempMesh.empty())
                return;
            const auto firstResolved = resolveTempPart(context, mount.tempMesh.front());
            const auto anchor = firstResolved ? partOrigin(context, *firstResolved) : Pos{0.0f, 0.0f, 0.0f};
            std::size_t extraAt = 0;
            for (std::size_t i = 1; i < mount.tempMesh.size() and extraAt < visual.extras.size(); ++i) {
                const auto resolved = resolveTempPart(context, mount.tempMesh[i]);
                if (not resolved)
                    continue;
                const auto extra = visual.extras[extraAt++];
                if (with<scene::Node>::exists(context, extra))
                    with<scene::Node>::modify(context, extra)->pose = mountPartPose(base, anchor, partOrigin(context, *resolved));
            }
        }

        void destroyMountMeshes(Writing context, scene::Root::Id root, scene::actor::Mesh::Id id, const std::vector<scene::actor::Mesh::Id>& extras) {
            destroyActor(context, root, id);
            for (const auto extra : extras)
                destroyActor(context, root, extra);
        }

        void setMountMeshesVisible(Writing context, scene::actor::Mesh::Id id, const std::vector<scene::actor::Mesh::Id>& extras, bool visible) {
            auto set = [&](scene::actor::Mesh::Id mesh) {
                if (with<scene::Node>::exists(context, mesh))
                    scene::Node::Actions::setVisible(context, mesh, visible);
            };
            set(id);
            for (const auto extra : extras)
                set(extra);
        }

        void tintMesh(Writing context, scene::actor::Mesh::Id id, RGB albedo, float opacity) {
            if (not with<scene::actor::MeshState>::exists(context, id))
                return;
            auto state = with<scene::actor::MeshState>::modify(context, id);
            state->albedo = albedo;
            state->opacity = opacity;
            scene::Node::Actions::setVisible(context, id, true);
        }

        void spawnBlueprintActors(Writing context, scene::Root::Id root, meshpack::Asset::Id interframe, const Blueprint& blueprint, Display display, std::vector<QuarkActor>& actors, auto&& spawnOne, bool filterByDisplay) {
            for (std::size_t cellIndex = 0; cellIndex < blueprint.cells.size(); ++cellIndex) {
                const auto& cell = blueprint.cells[cellIndex];
                const auto cellY = cell.placement.cell.y;
                if (not filterByDisplay or display.skeleton) {
                    for (std::size_t i = 0; i < cell.corners.size(); ++i) {
                        const auto& knot = cell.corners[i];
                        const auto resolved = resolveKnot(context, interframe, knot.kind);
                        if (not resolved) {
                            base::message("eltanin blueprints geometry: knot mesh missing for kind");
                            continue;
                        }
                        const auto origin = entryOrigin(context, *resolved);
                        if (not origin)
                            continue;
                        const auto world = mech::skeleton::worldPose(cell.placement, knot.ori);
                        if (const auto id = spawnOne(actorPose(world, *origin), *resolved))
                            actors.push_back(QuarkActor{.id = *id, .kind = QuarkActor::Kind::knot, .cell = cellIndex, .index = i, .cellY = cellY});
                    }
                    for (std::size_t i = 0; i < cell.halfribs.size(); ++i) {
                        const auto& halfChord = cell.halfribs[i];
                        const auto resolved = resolveHalfChord(context, interframe, halfChord.kind, halfChord.pole);
                        if (not resolved) {
                            base::message("eltanin blueprints geometry: half-chord mesh missing");
                            continue;
                        }
                        const auto origin = entryOrigin(context, *resolved);
                        if (not origin)
                            continue;
                        const auto world = mech::skeleton::worldPose(cell.placement, halfChord.ori);
                        if (const auto id = spawnOne(actorPose(world, *origin), *resolved))
                            actors.push_back(QuarkActor{.id = *id, .kind = QuarkActor::Kind::halfChord, .cell = cellIndex, .index = i, .cellY = cellY});
                    }
                }
                if (not filterByDisplay or display.membranes) {
                    for (std::size_t i = 0; i < cell.membranes.size(); ++i) {
                        const auto& wall = cell.membranes[i];
                        const auto resolved = resolveWall(context, interframe, wall.kind);
                        if (not resolved) {
                            base::message("eltanin blueprints geometry: wall mesh missing");
                            continue;
                        }
                        const auto origin = entryOrigin(context, *resolved);
                        if (not origin)
                            continue;
                        const auto world = mech::skeleton::worldPose(cell.placement, wall.ori);
                        if (const auto id = spawnOne(actorPose(world, *origin), *resolved))
                            actors.push_back(QuarkActor{.id = *id, .kind = QuarkActor::Kind::wall, .cell = cellIndex, .index = i, .cellY = cellY});
                    }
                }
            }
        }

    } // namespace

    auto localSeatFromOrigin(Pos origin) -> mech::cube::Corner {
        const auto bit = mech::space::ivec3{
            origin.x >= 0.0f ? 1 : 0,
            origin.y >= 0.0f ? 1 : 0,
            origin.z >= 0.0f ? 1 : 0,
        };
        for (std::size_t i = 0; i < mech::cube::corners.size(); ++i) {
            if (mech::cube::corners[i] == bit)
                return static_cast<mech::cube::Corner>(i);
        }
        return 0;
    }

    auto actorPose(const mech::space::cell::Placement& quarkPose, Pos entryOrigin) -> Pose {
        const auto ori = static_cast<mech::space::orient::key>(quarkPose.ori);
        const auto localSeat = localSeatFromOrigin(entryOrigin);
        const auto seat = mech::space::orient::cornerIndex(ori, localSeat);
        const auto& corner = mech::cube::corners[static_cast<std::size_t>(seat)];
        const float edge = mech::space::local::edge2meters;
        const Pos position{
            (static_cast<float>(quarkPose.cell.x) + static_cast<float>(corner.x)) * edge,
            (static_cast<float>(quarkPose.cell.y) + static_cast<float>(corner.y)) * edge,
            (static_cast<float>(quarkPose.cell.z) + static_cast<float>(corner.z)) * edge,
        };
        const mat3 rotation = mat3(mech::space::orient::matrix[static_cast<std::size_t>(ori)]);
        return Pose{.position = position, .rotation = glm::normalize(glm::quat_cast(rotation))};
    }

    auto gridActorPose(const mech::space::Transform& transform) -> Pose {
        const auto rotationKey = static_cast<mech::space::orient::key>(transform.rotation);
        const mat3 rotation = mat3(mech::space::orient::matrix[static_cast<std::size_t>(rotationKey)]);
        const float edge = mech::space::local::edge2meters;
        const Pos position{static_cast<float>(transform.grid.x) * edge, static_cast<float>(transform.grid.y) * edge, static_cast<float>(transform.grid.z) * edge};
        return Pose{.position = position, .rotation = glm::normalize(glm::quat_cast(rotation))};
    }

    auto resolveKnot(Reading context, meshpack::Asset::Id pack, mech::skeleton::Corner::Kind kind) -> base::maybe<meshpack::Asset::Resolved> {
        return with<meshpack::Asset>::resolve(context, pack, cornerMesh(kind));
    }

    auto resolveHalfChord(Reading context, meshpack::Asset::Id pack, mech::skeleton::Halfrib::Kind kind, mech::skeleton::Halfrib::Pole pole) -> base::maybe<meshpack::Asset::Resolved> {
        return with<meshpack::Asset>::resolve(context, pack, halfEdgeMesh(kind, pole));
    }

    auto resolveWall(Reading context, meshpack::Asset::Id pack, mech::skeleton::Membrane::Kind kind) -> base::maybe<meshpack::Asset::Resolved> {
        return with<meshpack::Asset>::resolve(context, pack, membraneMesh(kind));
    }

    void clearActors(Writing context, scene::Root::Id root, std::vector<QuarkActor>& actors) {
        for (const auto& actor : actors)
            destroyActor(context, root, actor.id);
        actors.clear();
    }

    void clearMountActors(Writing context, scene::Root::Id root, std::vector<MountActor>& actors) {
        for (const auto& actor : actors)
            destroyMountMeshes(context, root, actor.id, actor.extras);
        actors.clear();
    }

    void clearPaletteActors(Writing context, scene::Root::Id root, std::vector<PaletteMountActor>& actors) {
        for (const auto& actor : actors) {
            destroyMountMeshes(context, root, actor.id, actor.extras);
            for (const auto ball : actor.balls)
                destroyActor(context, root, ball);
        }
        actors.clear();
    }

    void applyDisplay(Writing context, Display display, int currentFloor, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts) {
        for (const auto& actor : quarks) {
            if (not with<scene::Node>::exists(context, actor.id))
                continue;
            scene::Node::Actions::setVisible(context, actor.id, display.showsQuark(actor.kind, actor.cellY, currentFloor));
        }
        for (const auto& actor : mounts) {
            const bool show = display.showsMount(actor.layer, actor.cellYMin, actor.cellYMax, currentFloor);
            setMountMeshesVisible(context, actor.id, actor.extras, show);
        }
    }

    void syncActors(Writing context, scene::Root::Id root, meshpack::Asset::Id interframe, const Blueprint& blueprint, Display display, int currentFloor, std::vector<QuarkActor>& actors) {
        clearActors(context, root, actors);
        spawnBlueprintActors(context, root, interframe, blueprint, display, actors, [&](Pose pose, const meshpack::Asset::Resolved& resolved) { return spawnIdentified(context, root, pose, resolved); }, false);
        applyDisplay(context, display, currentFloor, actors, {});
    }

    void syncMountActors(Writing context, scene::Root::Id root, const Blueprint& blueprint, Display display, int currentFloor, const std::unordered_map<mech::Mount::Id, mech::Layer>& mountLayers, std::vector<MountActor>& actors) {
        clearMountActors(context, root, actors);
        for (std::size_t index = 0; index < blueprint.mounts.size(); ++index) {
            const auto& placed = blueprint.mounts[index];
            const auto mountId = with<::rmmr::resource::Assets>::find<::eltanin::mech::Mount>(context, placed.mount);
            if (not mountId) {
                base::message("eltanin blueprints geometry: mount '{}' missing", placed.mount.text());
                continue;
            }
            const auto& mount = with<::eltanin::mech::Mount>::get(context, *mountId);
            mech::Layer layer = mount.attachment.flatMounted() ? mech::Layer::externals : mech::Layer::internals;
            if (const auto found = mountLayers.find(*mountId); found != mountLayers.end())
                layer = found->second;
            if (mount.tempMesh.empty()) {
                base::message("eltanin blueprints geometry: mount '{}' tempMesh empty", placed.mount.text());
                continue;
            }
            if (not resolveTempPart(context, mount.tempMesh.front())) {
                base::message("eltanin blueprints geometry: mount '{}' pack '{}' entry '{}' missing", placed.mount.text(), mount.tempMesh.front().pack.text(), mount.tempMesh.front().entry);
                continue;
            }
            int cellYMin = placed.transform.grid.y;
            int cellYMax = cellYMin;
            if (const auto box = mountBounds::cellBox(mount.attachment, placed.transform)) {
                cellYMin = box->min.y;
                cellYMax = box->max.y;
            }
            const auto albedo = mountAlbedo(mount);
            if (const auto visual = spawnMountVisual(context, gridActorPose(placed.transform), mount, [&](Pose pose, const meshpack::Asset::Resolved& resolved) { return spawnIdentified(context, root, pose, resolved, albedo); }))
                actors.push_back(MountActor{.id = visual->id, .extras = visual->extras, .index = index, .layer = layer, .cellYMin = cellYMin, .cellYMax = cellYMax});
        }
        applyDisplay(context, display, currentFloor, {}, actors);
    }

    auto appendWallActor(Writing context, scene::Root::Id root, meshpack::Asset::Id interframe, std::size_t cellIndex, std::size_t membraneIndex, const mech::space::cell::Placement& placement, const mech::skeleton::Membrane& wall, Display display, int currentFloor, std::vector<QuarkActor>& actors) -> bool {
        const auto resolved = resolveWall(context, interframe, wall.kind);
        if (not resolved) {
            base::message("eltanin blueprints geometry: wall mesh missing");
            return false;
        }
        const auto origin = entryOrigin(context, *resolved);
        if (not origin)
            return false;
        const auto world = mech::skeleton::worldPose(placement, wall.ori);
        const auto id = spawnIdentified(context, root, actorPose(world, *origin), *resolved);
        if (not id)
            return false;
        const auto cellY = placement.cell.y;
        actors.push_back(QuarkActor{.id = *id, .kind = QuarkActor::Kind::wall, .cell = cellIndex, .index = membraneIndex, .cellY = cellY});
        scene::Node::Actions::setVisible(context, *id, display.showsQuark(QuarkActor::Kind::wall, cellY, currentFloor));
        return true;
    }

    void eraseWallActor(Writing context, scene::Root::Id root, std::size_t actorSlot, std::vector<QuarkActor>& actors) {
        if (actorSlot >= actors.size())
            return;
        const auto removed = actors[actorSlot];
        destroyActor(context, root, removed.id);
        actors.erase(actors.begin() + static_cast<std::ptrdiff_t>(actorSlot));
        if (removed.kind != QuarkActor::Kind::wall)
            return;
        for (auto& actor : actors) {
            if (actor.kind == QuarkActor::Kind::wall and actor.cell == removed.cell and actor.index > removed.index)
                --actor.index;
        }
    }

    void syncPaletteActors(Writing context, scene::Root::Id root, const std::vector<mech::Mount::Id>& mounts, std::vector<PaletteMountActor>& actors, ::rmmr::resource::geometry::Asset::Id sphere, ::rmmr::resource::material::Asset::Id ballMaterial) {
        clearPaletteActors(context, root, actors);
        constexpr int columns = 4;
        constexpr int cellStep = 2;
        constexpr float ballOpacity = 0.55f;
        constexpr float ballScale = 0.35f;
        const RGB firstBallAlbedo{0.15f, 0.95f, 0.35f};
        const RGB otherBallAlbedo{1.0f, 0.72f, 0.22f};
        const float edge = mech::space::local::edge2meters;
        const auto ballResolved = meshpack::Asset::Resolved{
            .geometry = sphere,
            .entry = ::rmmr::resource::geometry::EntryId{0},
            .surfaces = {{::rmmr::resource::geometry::SurfaceId{0}, ::rmmr::resource::material::Instance{.material = ballMaterial, .textures = {}}}},
            .texpack = {},
        };
        for (std::size_t index = 0; index < mounts.size(); ++index) {
            const auto mountId = mounts[index];
            if (not with<::eltanin::mech::Mount>::exists(context, mountId))
                continue;
            const auto& mount = with<::eltanin::mech::Mount>::get(context, mountId);
            if (mount.tempMesh.empty()) {
                base::message("eltanin blueprints geometry: palette tempMesh empty");
                continue;
            }
            if (not resolveTempPart(context, mount.tempMesh.front())) {
                base::message("eltanin blueprints geometry: palette pack '{}' entry '{}' missing", mount.tempMesh.front().pack.text(), mount.tempMesh.front().entry);
                continue;
            }
            const auto col = static_cast<int>(index % static_cast<std::size_t>(columns));
            const auto row = static_cast<int>(index / static_cast<std::size_t>(columns));
            const auto transform = mech::space::Transform{.grid = base::common_types::index3{.x = col * cellStep, .y = 0, .z = row * cellStep}, .rotation = 0};
            const auto albedo = mountAlbedo(mount);
            const auto visual = spawnMountVisual(context, gridActorPose(transform), mount, [&](Pose pose, const meshpack::Asset::Resolved& resolved) { return spawnIdentified(context, root, pose, resolved, albedo); });
            if (visual) {
                std::vector<scene::actor::Mesh::Id> balls;
                balls.reserve(mount.attachment.points.size());
                for (std::size_t pointIndex = 0; pointIndex < mount.attachment.points.size(); ++pointIndex) {
                    const auto& point = mount.attachment.points[pointIndex];
                    const Pos world{
                        static_cast<float>(transform.grid.x + point.x) * edge,
                        static_cast<float>(transform.grid.y + point.y) * edge,
                        static_cast<float>(transform.grid.z + point.z) * edge,
                    };
                    const auto ballAlbedo = pointIndex == 0 ? firstBallAlbedo : otherBallAlbedo;
                    const auto ballId = with<scene::Interface>::createMeshActor(context, root, Pose::from(world, HPB{0.0f, 0.0f, 0.0f}), ballResolved, with<scene::actor::MeshState>::defaults(ballAlbedo, ballOpacity, vec3{ballScale, ballScale, ballScale}));
                    if (with<scene::actor::Mesh>::exists(context, ballId))
                        balls.push_back(ballId);
                }
                actors.push_back(PaletteMountActor{.id = visual->id, .extras = visual->extras, .mount = mountId, .balls = std::move(balls)});
            }
        }
    }

    void syncGhostActors(Writing context, scene::Root::Id root, meshpack::Asset::Id interframe, ::rmmr::resource::material::Asset::Id ghostMaterial, const Blueprint& blueprint, Display display, std::vector<QuarkActor>& actors, RGB albedo, float opacity) {
        clearActors(context, root, actors);
        spawnBlueprintActors(context, root, interframe, blueprint, display, actors, [&](Pose pose, const meshpack::Asset::Resolved& resolved) { return spawnGhost(context, root, pose, resolved, ghostMaterial, albedo, opacity); }, true);
    }

    auto refreshGhostActors(Writing context, meshpack::Asset::Id interframe, const Blueprint& blueprint, Display display, std::vector<QuarkActor>& actors, RGB albedo, float opacity) -> bool {
        std::size_t at = 0;
        const auto touch = [&](QuarkActor::Kind kind, std::size_t cell, std::size_t index, const mech::space::cell::Placement& world, const meshpack::Asset::Resolved& resolved) -> bool {
            const auto origin = entryOrigin(context, resolved);
            if (not origin)
                return false;
            if (at >= actors.size())
                return false;
            auto& slot = actors[at];
            if (slot.kind != kind or slot.cell != cell or slot.index != index)
                return false;
            if (not with<scene::Node>::exists(context, slot.id) or not with<scene::actor::Mesh>::exists(context, slot.id) or not with<scene::actor::MeshState>::exists(context, slot.id))
                return false;
            with<scene::Node>::modify(context, slot.id)->pose = actorPose(world, *origin);
            auto state = with<scene::actor::MeshState>::modify(context, slot.id);
            state->albedo = albedo;
            state->opacity = opacity;
            scene::Node::Actions::setVisible(context, slot.id, true);
            ++at;
            return true;
        };
        for (std::size_t cellIndex = 0; cellIndex < blueprint.cells.size(); ++cellIndex) {
            const auto& cell = blueprint.cells[cellIndex];
            if (display.skeleton) {
                for (std::size_t i = 0; i < cell.corners.size(); ++i) {
                    const auto& knot = cell.corners[i];
                    const auto resolved = resolveKnot(context, interframe, knot.kind);
                    if (not resolved)
                        continue;
                    if (not touch(QuarkActor::Kind::knot, cellIndex, i, mech::skeleton::worldPose(cell.placement, knot.ori), *resolved))
                        return false;
                }
                for (std::size_t i = 0; i < cell.halfribs.size(); ++i) {
                    const auto& halfChord = cell.halfribs[i];
                    const auto resolved = resolveHalfChord(context, interframe, halfChord.kind, halfChord.pole);
                    if (not resolved)
                        continue;
                    if (not touch(QuarkActor::Kind::halfChord, cellIndex, i, mech::skeleton::worldPose(cell.placement, halfChord.ori), *resolved))
                        return false;
                }
            }
            if (display.membranes) {
                for (std::size_t i = 0; i < cell.membranes.size(); ++i) {
                    const auto& wall = cell.membranes[i];
                    const auto resolved = resolveWall(context, interframe, wall.kind);
                    if (not resolved)
                        continue;
                    if (not touch(QuarkActor::Kind::wall, cellIndex, i, mech::skeleton::worldPose(cell.placement, wall.ori), *resolved))
                        return false;
                }
            }
        }
        return at == actors.size();
    }

    void syncGhostMountActors(Writing context, scene::Root::Id root, ::rmmr::resource::material::Asset::Id ghostMaterial, const Blueprint& blueprint, Display display, std::vector<MountActor>& actors, RGB albedo, float opacity) {
        clearMountActors(context, root, actors);
        for (std::size_t index = 0; index < blueprint.mounts.size(); ++index) {
            const auto& placed = blueprint.mounts[index];
            const auto mountId = with<::rmmr::resource::Assets>::find<::eltanin::mech::Mount>(context, placed.mount);
            if (not mountId)
                continue;
            const auto& mount = with<::eltanin::mech::Mount>::get(context, *mountId);
            const auto layer = mount.attachment.flatMounted() ? mech::Layer::externals : mech::Layer::internals;
            if (not display.shows(layer))
                continue;
            int cellYMin = placed.transform.grid.y;
            int cellYMax = cellYMin;
            if (const auto box = mountBounds::cellBox(mount.attachment, placed.transform)) {
                cellYMin = box->min.y;
                cellYMax = box->max.y;
            }
            if (const auto visual = spawnMountVisual(context, gridActorPose(placed.transform), mount, [&](Pose pose, const meshpack::Asset::Resolved& resolved) { return spawnGhost(context, root, pose, resolved, ghostMaterial, albedo, opacity); }))
                actors.push_back(MountActor{.id = visual->id, .extras = visual->extras, .index = index, .layer = layer, .cellYMin = cellYMin, .cellYMax = cellYMax});
        }
    }

    auto refreshGhostMountActors(Writing context, const Blueprint& blueprint, Display display, std::vector<MountActor>& actors, RGB albedo, float opacity) -> bool {
        std::size_t at = 0;
        for (std::size_t index = 0; index < blueprint.mounts.size(); ++index) {
            const auto& placed = blueprint.mounts[index];
            const auto mountId = with<::rmmr::resource::Assets>::find<::eltanin::mech::Mount>(context, placed.mount);
            if (not mountId)
                continue;
            const auto& mount = with<::eltanin::mech::Mount>::get(context, *mountId);
            const auto layer = mount.attachment.flatMounted() ? mech::Layer::externals : mech::Layer::internals;
            if (not display.shows(layer))
                continue;
            if (at >= actors.size())
                return false;
            auto& slot = actors[at];
            if (slot.index != index or slot.layer != layer)
                return false;
            if (not with<scene::Node>::exists(context, slot.id) or not with<scene::actor::MeshState>::exists(context, slot.id))
                return false;
            poseMountVisual(context, MountVisual{.id = slot.id, .extras = slot.extras}, mount, gridActorPose(placed.transform));
            tintMesh(context, slot.id, albedo, opacity);
            for (const auto extra : slot.extras)
                tintMesh(context, extra, albedo, opacity);
            ++at;
        }
        return at == actors.size();
    }

    void destroyMeshActor(Writing context, scene::Root::Id root, scene::actor::Mesh::Id actor) {
        destroyActor(context, root, actor);
    }

    void destroyMountVisual(Writing context, scene::Root::Id root, const MountVisual& visual) {
        destroyMountMeshes(context, root, visual.id, visual.extras);
    }

    auto spawnGhostMount(Writing context, scene::Root::Id root, ::rmmr::resource::material::Asset::Id ghostMaterial, mech::Mount::Id mountId, const mech::space::Transform& transform, RGB albedo, float opacity) -> base::maybe<MountVisual> {
        if (not with<::eltanin::mech::Mount>::exists(context, mountId))
            return {};
        const auto& mount = with<::eltanin::mech::Mount>::get(context, mountId);
        return spawnMountVisual(context, gridActorPose(transform), mount, [&](Pose pose, const meshpack::Asset::Resolved& resolved) { return spawnGhost(context, root, pose, resolved, ghostMaterial, albedo, opacity); });
    }

    void poseGhostMount(Writing context, const MountVisual& visual, mech::Mount::Id mountId, const mech::space::Transform& transform) {
        const auto base = gridActorPose(transform);
        if (not with<::eltanin::mech::Mount>::exists(context, mountId)) {
            if (with<scene::Node>::exists(context, visual.id))
                with<scene::Node>::modify(context, visual.id)->pose = base;
            return;
        }
        poseMountVisual(context, visual, with<::eltanin::mech::Mount>::get(context, mountId), base);
    }

} // namespace eltanin::views::blueprints::geometry
