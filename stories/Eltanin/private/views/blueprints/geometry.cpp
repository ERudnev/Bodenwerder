#include "views/blueprints/geometry.h"

#include "mech/semantics/shapes.h"
#include "mech/semantics/subframe.h"

#include <rmmr/resources/geometry.q1.h>
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

        auto spawnGhost(Writing context, scene::Root::Id root, Pose pose, meshpack::Asset::Resolved resolved, ::rmmr::resource::material::Asset::Id ghostMaterial, RGB albedo, float opacity) -> base::maybe<scene::actor::Mesh::Id> {
            for (auto& [_, surface] : resolved.surfaces)
                surface = ::rmmr::resource::material::Instance{.material = ghostMaterial, .textures = {}};
            const auto id = with<scene::Interface>::createMeshActor(context, root, pose, std::move(resolved), with<scene::actor::MeshState>::defaults(albedo, opacity));
            if (not with<scene::actor::Mesh>::exists(context, id))
                return {};
            return id;
        }

        void spawnBlueprintActors(Writing context, scene::Root::Id root, meshpack::Asset::Id interframe, const mech::Blueprint& blueprint, Display display, std::vector<QuarkActor>& actors, auto&& spawnOne) {
            for (std::size_t cellIndex = 0; cellIndex < blueprint.cells.size(); ++cellIndex) {
                const auto& cell = blueprint.cells[cellIndex];
                if (display.skeleton) {
                    for (std::size_t i = 0; i < cell.frame.corners.size(); ++i) {
                        const auto& knot = cell.frame.corners[i];
                        const auto resolved = resolveKnot(context, interframe, knot.kind);
                        if (not resolved) {
                            base::message("eltanin blueprints geometry: knot mesh missing for kind");
                            continue;
                        }
                        const auto origin = entryOrigin(context, *resolved);
                        if (not origin)
                            continue;
                        const auto world = mech::skeleton::worldPose(cell.pose, knot.ori);
                        if (const auto id = spawnOne(actorPose(world, *origin), *resolved))
                            actors.push_back(QuarkActor{.id = *id, .kind = QuarkActor::Kind::knot, .cell = cellIndex, .index = i});
                    }
                    for (std::size_t i = 0; i < cell.frame.halfribs.size(); ++i) {
                        const auto& halfChord = cell.frame.halfribs[i];
                        const auto resolved = resolveHalfChord(context, interframe, halfChord.kind, halfChord.pole);
                        if (not resolved) {
                            base::message("eltanin blueprints geometry: half-chord mesh missing");
                            continue;
                        }
                        const auto origin = entryOrigin(context, *resolved);
                        if (not origin)
                            continue;
                        const auto world = mech::skeleton::worldPose(cell.pose, halfChord.ori);
                        if (const auto id = spawnOne(actorPose(world, *origin), *resolved))
                            actors.push_back(QuarkActor{.id = *id, .kind = QuarkActor::Kind::halfChord, .cell = cellIndex, .index = i});
                    }
                }
                if (display.hull) {
                    for (std::size_t i = 0; i < cell.hull.membranes.size(); ++i) {
                        const auto& wall = cell.hull.membranes[i];
                        const auto resolved = resolveWall(context, interframe, wall.kind);
                        if (not resolved) {
                            base::message("eltanin blueprints geometry: wall mesh missing");
                            continue;
                        }
                        const auto origin = entryOrigin(context, *resolved);
                        if (not origin)
                            continue;
                        const auto world = mech::skeleton::worldPose(cell.pose, wall.ori);
                        if (const auto id = spawnOne(actorPose(world, *origin), *resolved))
                            actors.push_back(QuarkActor{.id = *id, .kind = QuarkActor::Kind::wall, .cell = cellIndex, .index = i});
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

    auto actorPose(const mech::space::cell::Pose& quarkPose, Pos entryOrigin) -> Pose {
        const auto ori = static_cast<mech::space::orient::key>(quarkPose.ori);
        const auto localSeat = localSeatFromOrigin(entryOrigin);
        const auto seat = mech::space::orient::cornerIndex(ori, localSeat);
        const auto& corner = mech::cube::corners[static_cast<std::size_t>(seat)];
        const float edge = mech::space::local::edge2meters;
        const Pos position{
            (static_cast<float>(quarkPose.pos.x) + static_cast<float>(corner.x)) * edge,
            (static_cast<float>(quarkPose.pos.y) + static_cast<float>(corner.y)) * edge,
            (static_cast<float>(quarkPose.pos.z) + static_cast<float>(corner.z)) * edge,
        };
        const mat3 rotation = mat3(mech::space::orient::matrix[static_cast<std::size_t>(ori)]);
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

    void syncActors(Writing context, scene::Root::Id root, meshpack::Asset::Id interframe, const mech::Blueprint& blueprint, Display display, std::vector<QuarkActor>& actors) {
        clearActors(context, root, actors);
        spawnBlueprintActors(context, root, interframe, blueprint, display, actors, [&](Pose pose, const meshpack::Asset::Resolved& resolved) { return spawnIdentified(context, root, pose, resolved); });
    }

    void syncGhostActors(Writing context, scene::Root::Id root, meshpack::Asset::Id interframe, ::rmmr::resource::material::Asset::Id ghostMaterial, const mech::Blueprint& blueprint, Display display, std::vector<QuarkActor>& actors, RGB albedo, float opacity) {
        clearActors(context, root, actors);
        spawnBlueprintActors(context, root, interframe, blueprint, display, actors, [&](Pose pose, const meshpack::Asset::Resolved& resolved) { return spawnGhost(context, root, pose, resolved, ghostMaterial, albedo, opacity); });
    }

    auto refreshGhostActors(Writing context, meshpack::Asset::Id interframe, const mech::Blueprint& blueprint, Display display, std::vector<QuarkActor>& actors, RGB albedo, float opacity) -> bool {
        std::size_t at = 0;
        const auto touch = [&](QuarkActor::Kind kind, std::size_t cell, std::size_t index, const mech::space::cell::Pose& world, const meshpack::Asset::Resolved& resolved) -> bool {
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
                for (std::size_t i = 0; i < cell.frame.corners.size(); ++i) {
                    const auto& knot = cell.frame.corners[i];
                    const auto resolved = resolveKnot(context, interframe, knot.kind);
                    if (not resolved)
                        continue;
                    if (not touch(QuarkActor::Kind::knot, cellIndex, i, mech::skeleton::worldPose(cell.pose, knot.ori), *resolved))
                        return false;
                }
                for (std::size_t i = 0; i < cell.frame.halfribs.size(); ++i) {
                    const auto& halfChord = cell.frame.halfribs[i];
                    const auto resolved = resolveHalfChord(context, interframe, halfChord.kind, halfChord.pole);
                    if (not resolved)
                        continue;
                    if (not touch(QuarkActor::Kind::halfChord, cellIndex, i, mech::skeleton::worldPose(cell.pose, halfChord.ori), *resolved))
                        return false;
                }
            }
            if (display.hull) {
                for (std::size_t i = 0; i < cell.hull.membranes.size(); ++i) {
                    const auto& wall = cell.hull.membranes[i];
                    const auto resolved = resolveWall(context, interframe, wall.kind);
                    if (not resolved)
                        continue;
                    if (not touch(QuarkActor::Kind::wall, cellIndex, i, mech::skeleton::worldPose(cell.pose, wall.ori), *resolved))
                        return false;
                }
            }
        }
        return at == actors.size();
    }

} // namespace eltanin::views::blueprints::geometry
