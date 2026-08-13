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
            if (mount.role.exists())
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
            destroyActor(context, root, actor.id);
        actors.clear();
    }

    void clearPaletteActors(Writing context, scene::Root::Id root, std::vector<PaletteMountActor>& actors) {
        for (const auto& actor : actors)
            destroyActor(context, root, actor.id);
        actors.clear();
    }

    void applyDisplay(Writing context, Display display, int currentFloor, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts) {
        for (const auto& actor : quarks) {
            if (not with<scene::actor::MeshState>::exists(context, actor.id))
                continue;
            scene::actor::MeshState::Actions::setVisible(context, actor.id, display.showsQuark(actor.kind, actor.cellY, currentFloor));
        }
        for (const auto& actor : mounts) {
            if (not with<scene::actor::MeshState>::exists(context, actor.id))
                continue;
            scene::actor::MeshState::Actions::setVisible(context, actor.id, display.showsMount(actor.layer, actor.cellYMin, actor.cellYMax, currentFloor));
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
            const auto packId = with<::rmmr::resource::Assets>::find<meshpack::Asset>(context, mount.tempMesh.pack);
            if (not packId) {
                base::message("eltanin blueprints geometry: mount '{}' pack '{}' missing", placed.mount.text(), mount.tempMesh.pack.text());
                continue;
            }
            const auto resolved = with<meshpack::Asset>::resolve(context, *packId, mount.tempMesh.entry);
            if (not resolved) {
                base::message("eltanin blueprints geometry: mount '{}' entry '{}' missing", placed.mount.text(), mount.tempMesh.entry);
                continue;
            }
            int cellYMin = placed.transform.grid.y;
            int cellYMax = cellYMin;
            if (const auto box = mountBounds::cellBox(mount.attachment, placed.transform)) {
                cellYMin = box->min.y;
                cellYMax = box->max.y;
            }
            if (const auto id = spawnIdentified(context, root, gridActorPose(placed.transform), *resolved, mountAlbedo(mount)))
                actors.push_back(MountActor{.id = *id, .index = index, .layer = layer, .cellYMin = cellYMin, .cellYMax = cellYMax});
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
        scene::actor::MeshState::Actions::setVisible(context, *id, display.showsQuark(QuarkActor::Kind::wall, cellY, currentFloor));
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

    void syncPaletteActors(Writing context, scene::Root::Id root, const std::vector<mech::Mount::Id>& mounts, std::vector<PaletteMountActor>& actors) {
        clearPaletteActors(context, root, actors);
        constexpr int columns = 4;
        constexpr int cellStep = 2;
        for (std::size_t index = 0; index < mounts.size(); ++index) {
            const auto mountId = mounts[index];
            if (not with<::eltanin::mech::Mount>::exists(context, mountId))
                continue;
            const auto& mount = with<::eltanin::mech::Mount>::get(context, mountId);
            const auto packId = with<::rmmr::resource::Assets>::find<meshpack::Asset>(context, mount.tempMesh.pack);
            if (not packId) {
                base::message("eltanin blueprints geometry: palette pack '{}' missing", mount.tempMesh.pack.text());
                continue;
            }
            const auto resolved = with<meshpack::Asset>::resolve(context, *packId, mount.tempMesh.entry);
            if (not resolved) {
                base::message("eltanin blueprints geometry: palette entry '{}' missing", mount.tempMesh.entry);
                continue;
            }
            const auto col = static_cast<int>(index % static_cast<std::size_t>(columns));
            const auto row = static_cast<int>(index / static_cast<std::size_t>(columns));
            const auto transform = mech::space::Transform{.grid = base::common_types::index3{.x = col * cellStep, .y = 0, .z = row * cellStep}, .rotation = 0};
            if (const auto id = spawnIdentified(context, root, gridActorPose(transform), *resolved, mountAlbedo(mount)))
                actors.push_back(PaletteMountActor{.id = *id, .mount = mountId});
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
            state->visible = true;
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
            const auto packId = with<::rmmr::resource::Assets>::find<meshpack::Asset>(context, mount.tempMesh.pack);
            if (not packId)
                continue;
            const auto resolved = with<meshpack::Asset>::resolve(context, *packId, mount.tempMesh.entry);
            if (not resolved)
                continue;
            int cellYMin = placed.transform.grid.y;
            int cellYMax = cellYMin;
            if (const auto box = mountBounds::cellBox(mount.attachment, placed.transform)) {
                cellYMin = box->min.y;
                cellYMax = box->max.y;
            }
            if (const auto id = spawnGhost(context, root, gridActorPose(placed.transform), *resolved, ghostMaterial, albedo, opacity))
                actors.push_back(MountActor{.id = *id, .index = index, .layer = layer, .cellYMin = cellYMin, .cellYMax = cellYMax});
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
            with<scene::Node>::modify(context, slot.id)->pose = gridActorPose(placed.transform);
            auto state = with<scene::actor::MeshState>::modify(context, slot.id);
            state->albedo = albedo;
            state->opacity = opacity;
            state->visible = true;
            ++at;
        }
        return at == actors.size();
    }

} // namespace eltanin::views::blueprints::geometry
