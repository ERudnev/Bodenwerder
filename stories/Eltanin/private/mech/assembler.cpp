#include "mech/assembler.h"

#include "mech/construction.h"
#include <eltanin/locality/thing.q1.h>
#include <eltanin/mech/mount.q1.h>
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>

#include "physics/settings.h"
#include "mech/semantics/physicalParameters.h"
#include "mech/semantics/quarks.h"
#include "mech/semantics/shapes.h"
#include "mech/semantics/space.h"
#include "mech/semantics/subframe.h"

#include <base/logging.h>

#include <algorithm>
#include <format>
#include <map>
#include <utility>

#include <glm/geometric.hpp>

namespace eltanin::mech {

    using namespace fqsm::api;
    using namespace rmmr;
    using Construct = locality::Construct;

    namespace {

        struct LatticeLess {
            auto operator()(const index3& left, const index3& right) const -> bool {
                if (left.x != right.x) return left.x < right.x;
                if (left.y != right.y) return left.y < right.y;
                return left.z < right.z;
            }
        };

        using EdgeKey = std::pair<index3, index3>;

        struct EdgeLess {
            auto operator()(const EdgeKey& left, const EdgeKey& right) const -> bool {
                if (LatticeLess{}(left.first, right.first)) return true;
                if (LatticeLess{}(right.first, left.first)) return false;
                return LatticeLess{}(left.second, right.second);
            }
        };

        auto sameLattice(const index3& a, const index3& b) -> bool {
            return a.x == b.x and a.y == b.y and a.z == b.z;
        }

        auto addLattice(const index3& cell, const ivec3& corner) -> index3 {
            return index3{.x = cell.x + corner.x, .y = cell.y + corner.y, .z = cell.z + corner.z};
        }

        auto canonicalEdge(index3 a, index3 b) -> EdgeKey {
            if (LatticeLess{}(b, a)) return EdgeKey{b, a};
            return EdgeKey{a, b};
        }

        auto cornerMesh(skeleton::Corner::Kind kind) -> std::string {
            return std::string{skeleton::cornerSpecs.at(kind).code};
        }

        auto halfribMesh(skeleton::Halfrib::Kind kind, skeleton::Halfrib::Pole pole) -> std::string {
            const char poleTag = pole == skeleton::Halfrib::Pole::starts ? 's' : 'e';
            return std::format("{}{}", skeleton::halfribSpecs.at(kind).code, poleTag);
        }

        auto membraneMesh(skeleton::Membrane::Kind kind) -> std::string {
            return std::string{skeleton::membraneSpecs.at(kind).code};
        }

        auto localSeatFromOrigin(vec3 origin) -> cube::Corner {
            const space::ivec3 bit{
                origin.x >= 0.0f ? 1 : 0,
                origin.y >= 0.0f ? 1 : 0,
                origin.z >= 0.0f ? 1 : 0,
            };
            for (std::size_t index = 0; index < cube::corners.size(); ++index) {
                if (cube::corners[index] == bit)
                    return static_cast<cube::Corner>(index);
            }
            return 0;
        }

        auto gridVertex(const space::cell::Placement& world, cube::Corner local) -> index3 {
            const auto seat = space::orient::cornerIndex(world.ori, local);
            return addLattice(world.cell, cube::corners[static_cast<std::size_t>(seat)]);
        }

        auto entryOrigin(Reading context, const resource::meshpack::Asset::Resolved& resolved) -> base::maybe<vec3> {
            if (not with<resource::geometry::Asset>::exists(context, resolved.geometry))
                return {};
            const auto& asset = with<resource::geometry::Asset>::get(context, resolved.geometry);
            if (resolved.entry >= asset.entries.size())
                return {};
            return asset.entries[resolved.entry].origin;
        }

        struct LoopLess {
            auto operator()(const vector<index3>& left, const vector<index3>& right) const -> bool {
                if (left.size() != right.size())
                    return left.size() < right.size();
                for (std::size_t i = 0; i < left.size(); ++i) {
                    if (LatticeLess{}(left[i], right[i]))
                        return true;
                    if (LatticeLess{}(right[i], left[i]))
                        return false;
                }
                return false;
            }
        };

        struct ShellLess {
            auto operator()(const vector<vector<index3>>& left, const vector<vector<index3>>& right) const -> bool {
                if (left.size() != right.size())
                    return left.size() < right.size();
                for (std::size_t index = 0; index < left.size(); ++index) {
                    if (LoopLess{}(left[index], right[index]))
                        return true;
                    if (LoopLess{}(right[index], left[index]))
                        return false;
                }
                return false;
            }
        };

        auto hasDuplicateVertex(const vector<index3>& loop) -> bool {
            for (std::size_t i = 0; i < loop.size(); ++i) {
                for (std::size_t j = i + 1; j < loop.size(); ++j) {
                    if (sameLattice(loop[i], loop[j]))
                        return true;
                }
            }
            return false;
        }

        auto cycleKey(vector<index3> loop) -> vector<index3> {
            if (loop.size() == 2)
                return LatticeLess{}(loop[1], loop[0]) ? vector<index3>{loop[1], loop[0]} : loop;
            if (loop.size() < 3)
                return loop;
            std::size_t minIndex = 0;
            for (std::size_t i = 1; i < loop.size(); ++i) {
                if (LatticeLess{}(loop[i], loop[minIndex]))
                    minIndex = i;
            }
            vector<index3> rotated;
            rotated.reserve(loop.size());
            for (std::size_t i = 0; i < loop.size(); ++i)
                rotated.push_back(loop[(minIndex + i) % loop.size()]);
            if (LatticeLess{}(rotated.back(), rotated[1]))
                std::reverse(rotated.begin() + 1, rotated.end());
            return rotated;
        }

        auto shellKey(const vector<vector<index3>>& worldFaces) -> vector<vector<index3>> {
            vector<vector<index3>> keys;
            keys.reserve(worldFaces.size());
            for (const auto& loop : worldFaces)
                keys.push_back(cycleKey(loop));
            std::sort(keys.begin(), keys.end(), LoopLess{});
            return keys;
        }

        auto worldLoop(const space::cell::Placement& world, skeleton::Membrane::Kind kind) -> vector<index3> {
            const auto& canonical = plate::perimeter[static_cast<std::size_t>(skeleton::plateOf(kind))];
            vector<index3> loop;
            loop.reserve(canonical.size());
            for (const auto corner : canonical)
                loop.push_back(gridVertex(world, corner));
            return loop;
        }

        auto rotateLocal(space::orient::key rotation, index3 local) -> index3 {
            const auto& matrix = space::orient::matrix[static_cast<std::size_t>(rotation)];
            const auto rotated = matrix * space::ivec3{local.x, local.y, local.z};
            return index3{.x = rotated.x, .y = rotated.y, .z = rotated.z};
        }

        auto worldPoint(const space::Transform& transform, index3 local) -> index3 {
            const auto rotated = rotateLocal(transform.rotation, local);
            return index3{.x = transform.grid.x + rotated.x, .y = transform.grid.y + rotated.y, .z = transform.grid.z + rotated.z};
        }

        auto worldFace(const space::Transform& transform, const Attachment& attachment, const vector<integer>& indices) -> vector<index3> {
            vector<index3> loop;
            loop.reserve(indices.size());
            for (const auto index : indices) {
                if (index < 0 or static_cast<std::size_t>(index) >= attachment.points.size())
                    return {};
                loop.push_back(worldPoint(transform, attachment.points[static_cast<std::size_t>(index)]));
            }
            return loop;
        }

        using Primitive = Construction::Primitive;
        using PrimitiveId = Primitive::Id;

        constexpr float weldUnit = 1.0f;

        auto weldedAt(index3 grid, float mass, float strength) -> Primitive::Welded {
            return Primitive::Welded{Primitive::Point{.gridPos = grid, .mass = mass}, strength};
        }

        auto primitiveOn(const vector<index3>& verts, float totalMass, float thickness, float strength) -> Primitive {
            Primitive primitive{.loop = {}, .thickness = thickness};
            const float share = verts.empty() ? 0.0f : totalMass / static_cast<float>(verts.size());
            primitive.loop.reserve(verts.size());
            for (const auto& grid : verts)
                primitive.loop.push_back(weldedAt(grid, share, strength));
            return primitive;
        }

        void addMass(Primitive& primitive, float totalMass) {
            if (primitive.loop.empty())
                return;
            const float share = totalMass / static_cast<float>(primitive.loop.size());
            for (auto& welded : primitive.loop)
                welded.mass += share;
        }

        auto glueFrame(Reading context, resource::meshpack::Asset::Id interframe, const Blueprint::Quantum& blueprint) -> std::pair<Construction, Construct::ActorFragments> {
            Construction construction{.knots = {}, .ribs = {}, .membranes = {}, .plates = {}, .volumes = {}, .evaluatedParticles = {}};
            Construct::ActorFragments fragments{.ofKnot = {}, .ofRib = {}, .ofMembrane = {}, .ofPlate = {}, .ofVolume = {}};
            integer nextId = 0;
            const auto takeId = [&]() { return static_cast<PrimitiveId>(nextId++); };
            std::map<index3, PrimitiveId, LatticeLess> knotsAt;
            std::map<EdgeKey, PrimitiveId, EdgeLess> ribsAt;
            std::map<vector<index3>, PrimitiveId, LoopLess> membranesAt;
            std::map<vector<index3>, PrimitiveId, LoopLess> platesAt;
            std::map<vector<vector<index3>>, PrimitiveId, ShellLess> volumesAt;

            for (const auto& cell : blueprint.cells) {
                for (const auto& corner : cell.corners) {
                    const auto world = skeleton::worldPose(cell.placement, corner.ori);
                    cube::Corner localSeat = 0;
                    const auto resolved = with<resource::meshpack::Asset>::resolve(context, interframe, cornerMesh(corner.kind));
                    if (resolved) {
                        if (const auto origin = entryOrigin(context, *resolved))
                            localSeat = localSeatFromOrigin(*origin);
                    } else {
                        base::message("eltanin::mech::Assembler::spawn: knot mesh missing for kind");
                    }
                    const auto grid = gridVertex(world, localSeat);
                    auto found = knotsAt.find(grid);
                    if (found == knotsAt.end()) {
                        const auto id = takeId();
                        construction.knots.emplace(id, primitiveOn({grid}, physical::knotQuarkMass.at(corner.kind), physical::knotShell, weldUnit));
                        found = knotsAt.emplace(grid, id).first;
                    } else {
                        addMass(construction.knots.at(found->second), physical::knotQuarkMass.at(corner.kind));
                    }
                    if (resolved)
                        fragments.ofKnot.push_back(Construct::ActorFragments::OfKnot{.knot = found->second, .quark = skeleton::Corner{.kind = corner.kind, .ori = world.ori}});
                }

                for (const auto& halfrib : cell.halfribs) {
                    const auto world = skeleton::worldPose(cell.placement, halfrib.ori);
                    const auto ray = skeleton::halfribSpecs.at(halfrib.kind).ray;
                    const auto edge = canonicalEdge(gridVertex(world, 0), gridVertex(world, static_cast<cube::Corner>(ray)));
                    auto found = ribsAt.find(edge);
                    if (found == ribsAt.end()) {
                        const auto id = takeId();
                        construction.ribs.emplace(id, primitiveOn({edge.first, edge.second}, physical::halfribQuarkMass.at(halfrib.kind), physical::ribShell, weldUnit));
                        found = ribsAt.emplace(edge, id).first;
                    } else {
                        addMass(construction.ribs.at(found->second), physical::halfribQuarkMass.at(halfrib.kind));
                    }
                    const auto resolved = with<resource::meshpack::Asset>::resolve(context, interframe, halfribMesh(halfrib.kind, halfrib.pole));
                    if (not resolved) {
                        base::message("eltanin::mech::Assembler::spawn: halfrib mesh missing");
                        continue;
                    }
                    cube::Corner localSeat = 0;
                    if (const auto origin = entryOrigin(context, *resolved))
                        localSeat = localSeatFromOrigin(*origin);
                    fragments.ofRib.push_back(Construct::ActorFragments::OfRib{
                        .rib = found->second,
                        .quark = skeleton::Halfrib{.kind = halfrib.kind, .pole = halfrib.pole, .ori = world.ori},
                        .at = gridVertex(world, localSeat),
                    });
                }
            }

            for (const auto& cell : blueprint.cells) {
                for (const auto& membrane : cell.membranes) {
                    const auto world = skeleton::worldPose(cell.placement, membrane.ori);
                    auto loop = worldLoop(world, membrane.kind);
                    if (loop.size() < 3 or hasDuplicateVertex(loop))
                        continue;
                    const auto key = cycleKey(loop);
                    auto found = membranesAt.find(key);
                    if (found == membranesAt.end()) {
                        const auto id = takeId();
                        construction.membranes.emplace(id, primitiveOn(loop, physical::membraneQuarkMass.at(membrane.kind), physical::membraneShell, weldUnit));
                        found = membranesAt.emplace(key, id).first;
                    } else {
                        addMass(construction.membranes.at(found->second), physical::membraneQuarkMass.at(membrane.kind));
                    }
                    const auto resolved = with<resource::meshpack::Asset>::resolve(context, interframe, membraneMesh(membrane.kind));
                    if (not resolved) {
                        base::message("eltanin::mech::Assembler::spawn: membrane mesh missing");
                        continue;
                    }
                    cube::Corner localSeat = 0;
                    if (const auto origin = entryOrigin(context, *resolved))
                        localSeat = localSeatFromOrigin(*origin);
                    fragments.ofMembrane.push_back(Construct::ActorFragments::OfMembrane{
                        .membrane = found->second,
                        .quark = skeleton::Membrane{.kind = membrane.kind, .ori = world.ori},
                        .at = gridVertex(world, localSeat),
                    });
                }
            }

            for (const auto& placed : blueprint.mounts) {
                const auto mountId = with<resource::Assets>::find<Mount>(context, placed.mount);
                if (not mountId) {
                    base::message("eltanin::mech::Assembler::spawn: mount '{}' missing", placed.mount.text());
                    continue;
                }
                const auto& mount = with<Mount>::get(context, *mountId);
                vector<vector<index3>> worldFaces;
                worldFaces.reserve(mount.collision.faces.size());
                for (const auto& indices : mount.collision.faces) {
                    auto loop = worldFace(placed.transform, mount.attachment, indices);
                    if (loop.size() < 2 or hasDuplicateVertex(loop))
                        continue;
                    worldFaces.push_back(std::move(loop));
                }
                if (worldFaces.size() == 1) {
                    auto loop = std::move(worldFaces.front());
                    const auto key = cycleKey(loop);
                    auto found = platesAt.find(key);
                    if (found == platesAt.end()) {
                        const auto id = takeId();
                        construction.plates.emplace(id, primitiveOn(loop, mount.mass, mount.collision.thickness, weldUnit));
                        found = platesAt.emplace(key, id).first;
                    } else {
                        auto& plate = construction.plates.at(found->second);
                        addMass(plate, mount.mass);
                        if (mount.collision.thickness > plate.thickness)
                            plate.thickness = mount.collision.thickness;
                    }
                    fragments.ofPlate.push_back(Construct::ActorFragments::OfPlate{.plate = found->second, .mount = placed.mount, .transform = placed.transform});
                    continue;
                }
                if (worldFaces.empty())
                    continue;
                const auto key = shellKey(worldFaces);
                auto found = volumesAt.find(key);
                if (found == volumesAt.end()) {
                    const auto id = takeId();
                    vector<Primitive> faces;
                    faces.reserve(worldFaces.size());
                    const float faceMass = mount.mass / static_cast<float>(worldFaces.size());
                    for (const auto& loop : worldFaces)
                        faces.push_back(primitiveOn(loop, faceMass, mount.collision.thickness, weldUnit));
                    construction.volumes.emplace(id, std::move(faces));
                    found = volumesAt.emplace(key, id).first;
                } else {
                    const float faceMass = mount.mass / static_cast<float>(worldFaces.size());
                    for (auto& face : construction.volumes.at(found->second)) {
                        addMass(face, faceMass);
                        if (mount.collision.thickness > face.thickness)
                            face.thickness = mount.collision.thickness;
                    }
                }
                fragments.ofVolume.push_back(Construct::ActorFragments::OfVolume{.volume = found->second, .mount = placed.mount, .transform = placed.transform});
            }

            compileParticles(construction);
            return {std::move(construction), std::move(fragments)};
        }

        auto crystalFrom(const Construction& construction, Pose pose, vec3 velocity) -> phys::rigid::Crystal::Quantum {
            const auto count = construction.evaluatedParticles.size();
            vector<vec3> shape(count, vec3{0.0f, 0.0f, 0.0f});
            vector<phys::Particle> particles(count, phys::Particle{phys::Matter{.position = dvec3{0.0, 0.0, 0.0}, .mass = 1.0f, .temperature = 0.0f, .cohesion = 0.5f}, dvec3{0.0, 0.0, 0.0}, vec3{0.0f, 0.0f, 0.0f}});
            glm::dvec3 moment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (std::size_t index = 0; index < count; ++index) {
                const auto& point = construction.evaluatedParticles[index];
                const vec3 local{static_cast<float>(point.gridPos.x), static_cast<float>(point.gridPos.y), static_cast<float>(point.gridPos.z)};
                const vec3 meters = local * space::local::edge2meters;
                const vec3 world = pose.position + pose.rotation * meters;
                shape[index] = meters;
                particles[index] = phys::Particle{phys::Matter{.position = dvec3{world}, .mass = point.mass, .temperature = 0.0f, .cohesion = 1.0f}, dvec3{world} - dvec3{velocity * float(phys::Settings::fixedStep)}, vec3{0.0f, 0.0f, 0.0f}};
                moment += glm::dvec3{meters} * double(point.mass);
                mass += double(point.mass);
            }
            auto hull = cookHull(construction, shape);
            return phys::rigid::Crystal::Quantum{
                .particles = std::move(particles),
                .shape = std::move(shape),
                .com = mass > 0.0 ? vec3{moment / mass} : vec3{0.0f, 0.0f, 0.0f},
                .hull = std::move(hull),
            };
        }

    }

    auto cookOccurrences(Reading context, resource::meshpack::Asset::Id interframe, const Construction& construction, const locality::Construct::ActorFragments& fragments, vector<Construction::Primitive::Id>& visualOf) -> vector<scene::actor::Mesh::Occurrence> {
        vector<scene::actor::Mesh::Occurrence> occurrences;
        visualOf.clear();
        occurrences.reserve(fragments.ofKnot.size() + fragments.ofRib.size() + fragments.ofMembrane.size() + fragments.ofPlate.size() + fragments.ofVolume.size());
        visualOf.reserve(occurrences.capacity());
        for (const auto& piece : fragments.ofKnot) {
            const auto resolved = with<resource::meshpack::Asset>::resolve(context, interframe, cornerMesh(piece.quark.kind));
            if (not resolved)
                continue;
            const auto knot = construction.knots.find(piece.knot);
            if (knot == construction.knots.end() or knot->second.loop.empty())
                continue;
            occurrences.push_back(scene::actor::Mesh::Occurrence{.entry = *resolved, .pose = renderer::DiscretePose{.pos = knot->second.loop[0].gridPos, .ori = piece.quark.ori}});
            visualOf.push_back(piece.knot);
        }
        for (const auto& piece : fragments.ofRib) {
            const auto resolved = with<resource::meshpack::Asset>::resolve(context, interframe, halfribMesh(piece.quark.kind, piece.quark.pole));
            if (not resolved)
                continue;
            occurrences.push_back(scene::actor::Mesh::Occurrence{.entry = *resolved, .pose = renderer::DiscretePose{.pos = piece.at, .ori = piece.quark.ori}});
            visualOf.push_back(piece.rib);
        }
        for (const auto& piece : fragments.ofMembrane) {
            const auto resolved = with<resource::meshpack::Asset>::resolve(context, interframe, membraneMesh(piece.quark.kind));
            if (not resolved)
                continue;
            occurrences.push_back(scene::actor::Mesh::Occurrence{.entry = *resolved, .pose = renderer::DiscretePose{.pos = piece.at, .ori = piece.quark.ori}});
            visualOf.push_back(piece.membrane);
        }
        for (const auto& piece : fragments.ofPlate) {
            const auto mountId = with<resource::Assets>::find<Mount>(context, piece.mount);
            if (not mountId)
                continue;
            const auto& mount = with<Mount>::get(context, *mountId);
            const auto packId = with<resource::Assets>::find<resource::meshpack::Asset>(context, mount.tempMesh.pack);
            if (not packId)
                continue;
            const auto resolved = with<resource::meshpack::Asset>::resolve(context, *packId, mount.tempMesh.entry);
            if (not resolved)
                continue;
            occurrences.push_back(scene::actor::Mesh::Occurrence{.entry = *resolved, .pose = renderer::DiscretePose{.pos = piece.transform.grid, .ori = piece.transform.rotation}});
            visualOf.push_back(piece.plate);
        }
        for (const auto& piece : fragments.ofVolume) {
            const auto mountId = with<resource::Assets>::find<Mount>(context, piece.mount);
            if (not mountId)
                continue;
            const auto& mount = with<Mount>::get(context, *mountId);
            const auto packId = with<resource::Assets>::find<resource::meshpack::Asset>(context, mount.tempMesh.pack);
            if (not packId)
                continue;
            const auto resolved = with<resource::meshpack::Asset>::resolve(context, *packId, mount.tempMesh.entry);
            if (not resolved)
                continue;
            occurrences.push_back(scene::actor::Mesh::Occurrence{.entry = *resolved, .pose = renderer::DiscretePose{.pos = piece.transform.grid, .ori = piece.transform.rotation}});
            visualOf.push_back(piece.volume);
        }
        return occurrences;
    }

    auto Assembler::spawn(Writing context, Pose pose, Blueprint::Id blueprintId, vec3 velocity) -> locality::Construct::Id {
        const auto scene = with<locality::Thing>::get_global(context).scene;
        if (not with<Blueprint>::exists(context, blueprintId))
            return context.refuse("eltanin::mech::Assembler::spawn: blueprint missing");
        const auto& resources = with<Construct>::get_global(context).resources;
        if (not resources)
            return context.refuse("eltanin::mech::Assembler::spawn: Construct resources not bound");
        const auto interframe = resources->interframe;

        const auto& blueprint = with<Blueprint>::get(context, blueprintId);
        auto [construction, fragments] = glueFrame(context, interframe, blueprint);
        if (construction.knots.empty())
            return context.refuse("eltanin::mech::Assembler::spawn: blueprint has no knots");

        vector<Construction::Primitive::Id> visualOf;
        auto occurrences = cookOccurrences(context, interframe, construction, fragments, visualOf);
        if (occurrences.empty())
            return context.refuse("eltanin::mech::Assembler::spawn: no quark meshes to compose");
        auto meshQuantum = with<scene::actor::Mesh>::compose(context, occurrences);
        if (not meshQuantum)
            return context.refuse("eltanin::mech::Assembler::spawn: mesh compose failed");
        auto meshState = with<scene::actor::MeshState>::defaults();
        meshState.latticeStep = space::local::edge2meters;
        const auto actor = with<scene::Interface>::createMeshActor(context, scene, pose, std::move(*meshQuantum), std::move(meshState));

        auto crystal = crystalFrom(construction, pose, velocity);
        const auto body = phys::createBody(context, phys::rigid::restoredBody(pose, crystal.particles, crystal.shape), {});
        with<phys::rigid::Crystal>::extend(context, body, std::move(crystal));

        const auto thing = with<locality::Thing>::create(context, locality::Thing::Quantum{.bornAt = with<locality::Thing>::get_global(context).now});
        with<Construct>::extend(context, thing, Construct::Quantum{.body = body, .actor = actor, .fragments = std::move(fragments), .construction = std::move(construction), .visualOf = std::move(visualOf), .gpuCohesions = {}, .gpuHeats = {}});
        with<Construct>::syncVisualCohesion(context, thing);
        return thing;
    }

}
