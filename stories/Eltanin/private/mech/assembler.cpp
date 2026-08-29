#include "mech/assembler.h"

#include <eltanin/mech/mount.q1.h>
#include <eltanin/physics/compound.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>

#include "mech/semantics/physicalParameters.h"
#include "mech/semantics/quarks.h"
#include "mech/semantics/shapes.h"
#include "mech/semantics/space.h"
#include "mech/semantics/subframe.h"

#include <base/logging.h>

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <utility>

#include <glm/geometric.hpp>

namespace eltanin::mech {

    using namespace fqsm::api;
    using namespace rmmr;

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

        auto sortedKnots(vector<integer> points) -> vector<integer> {
            std::sort(points.begin(), points.end());
            return points;
        }

        auto beamFace(integer start, integer end, const vector<vec3>& shape, float thickness) -> phys::rigid::Hull::Face {
            const vec3 edge = shape[static_cast<std::size_t>(end)] - shape[static_cast<std::size_t>(start)];
            const vec3 mid = 0.5f * (shape[static_cast<std::size_t>(start)] + shape[static_cast<std::size_t>(end)]);
            vec3 normal = glm::cross(edge, mid);
            if (glm::dot(normal, normal) < 1.0e-12f)
                normal = glm::cross(edge, vec3{1.0f, 0.0f, 0.0f});
            if (glm::dot(normal, normal) < 1.0e-12f)
                normal = glm::cross(edge, vec3{0.0f, 1.0f, 0.0f});
            const float mag = glm::length(normal);
            if (mag > 1.0e-12f)
                normal /= mag;
            else
                normal = vec3{0.0f, 1.0f, 0.0f};
            return phys::rigid::Hull::Face{.points = {start, end}, .normal = normal, .thickness = thickness, .twoSided = false};
        }

        auto plateFace(const vector<integer>& points, const vector<vec3>& shape, float thickness) -> phys::rigid::Hull::Face {
            if (points.size() < 3)
                return phys::rigid::Hull::Face{.points = {}, .normal = vec3{0.0f, 1.0f, 0.0f}, .thickness = thickness, .twoSided = false};
            const vec3 ab = shape[static_cast<std::size_t>(points[1])] - shape[static_cast<std::size_t>(points[0])];
            const vec3 ac = shape[static_cast<std::size_t>(points[2])] - shape[static_cast<std::size_t>(points[0])];
            vec3 normal = glm::cross(ab, ac);
            const float mag = glm::length(normal);
            if (mag <= 1.0e-12f)
                return phys::rigid::Hull::Face{.points = {}, .normal = vec3{0.0f, 1.0f, 0.0f}, .thickness = thickness, .twoSided = false};
            normal /= mag;
            return phys::rigid::Hull::Face{.points = points, .normal = normal, .thickness = thickness, .twoSided = true};
        }

        auto hullFace(const vector<integer>& points, const vector<vec3>& shape, float thickness) -> phys::rigid::Hull::Face {
            if (points.size() == 2)
                return beamFace(points[0], points[1], shape, thickness);
            return plateFace(points, shape, thickness);
        }

        using Primitive = Construction::Primitive;
        using PrimitiveId = Primitive::Id;

        constexpr float weldUnit = 1.0f;

        auto weldedAt(index3 grid, float mass, float strength) -> Primitive::Welded {
            return Primitive::Welded{Primitive::Point{.gridPos = grid, .mass = mass, .particle = -1}, strength};
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

        auto loopGrid(const Primitive& primitive) -> vector<index3> {
            vector<index3> grid;
            grid.reserve(primitive.loop.size());
            for (const auto& welded : primitive.loop)
                grid.push_back(welded.gridPos);
            return grid;
        }

        void emitLoop(Primitive& primitive, Construction& construction) {
            for (auto& welded : primitive.loop) {
                const auto index = static_cast<integer>(construction.evaluatedParticles.size());
                welded.particle = index;
                construction.evaluatedParticles.push_back(Primitive::Point{.gridPos = welded.gridPos, .mass = welded.mass, .particle = index});
            }
        }

        auto sortedPrimitiveIds(const auto& items) -> vector<PrimitiveId> {
            vector<PrimitiveId> ids;
            ids.reserve(items.size());
            for (const auto& [id, _] : items)
                ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            return ids;
        }

        void compileParticles(Construction& construction) {
            construction.evaluatedParticles.clear();
            for (const auto id : sortedPrimitiveIds(construction.knots))
                emitLoop(construction.knots.at(id), construction);
            for (const auto id : sortedPrimitiveIds(construction.ribs))
                emitLoop(construction.ribs.at(id), construction);
            for (const auto id : sortedPrimitiveIds(construction.membranes))
                emitLoop(construction.membranes.at(id), construction);
            for (const auto id : sortedPrimitiveIds(construction.plates))
                emitLoop(construction.plates.at(id), construction);
            for (const auto id : sortedPrimitiveIds(construction.volumes)) {
                for (auto& face : construction.volumes.at(id))
                    emitLoop(face, construction);
            }
        }

        void addLoopEdges(const Primitive& primitive, std::set<EdgeKey, EdgeLess>& covered) {
            const auto grid = loopGrid(primitive);
            if (grid.size() == 2) {
                covered.insert(canonicalEdge(grid[0], grid[1]));
                return;
            }
            if (grid.size() < 3)
                return;
            for (std::size_t index = 0; index < grid.size(); ++index)
                covered.insert(canonicalEdge(grid[index], grid[(index + 1) % grid.size()]));
        }

        auto particlePoints(const Primitive& primitive) -> vector<integer> {
            vector<integer> points;
            points.reserve(primitive.loop.size());
            for (const auto& welded : primitive.loop) {
                if (welded.particle < 0)
                    return {};
                points.push_back(welded.particle);
            }
            return points;
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

        auto cookOccurrences(Reading context, resource::meshpack::Asset::Id interframe, const Construction& construction, const Construct::ActorFragments& fragments, vector<Construction::Primitive::Id>& visualOf) -> vector<scene::actor::Mesh::Occurrence> {
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
                particles[index] = phys::Particle{phys::Matter{.position = dvec3{world}, .mass = point.mass, .temperature = 0.0f, .cohesion = 1.0f}, dvec3{world} - dvec3{velocity * phys::Particle::dt}, vec3{0.0f, 0.0f, 0.0f}};
                moment += glm::dvec3{meters} * double(point.mass);
                mass += double(point.mass);
            }
            std::set<EdgeKey, EdgeLess> covered;
            for (const auto& [_, primitive] : construction.membranes)
                addLoopEdges(primitive, covered);
            for (const auto& [_, primitive] : construction.plates)
                addLoopEdges(primitive, covered);
            for (const auto& [_, faces] : construction.volumes) {
                for (const auto& primitive : faces)
                    addLoopEdges(primitive, covered);
            }
            phys::rigid::Hull hull{.faces = {}};
            hull.faces.reserve(construction.ribs.size() + construction.membranes.size() + construction.plates.size());
            for (const auto& [_, rib] : construction.ribs) {
                const auto grid = loopGrid(rib);
                if (grid.size() != 2 or sameLattice(grid[0], grid[1]))
                    continue;
                if (covered.find(canonicalEdge(grid[0], grid[1])) != covered.end())
                    continue;
                const auto points = particlePoints(rib);
                if (points.size() != 2)
                    continue;
                hull.faces.push_back(beamFace(points[0], points[1], shape, rib.thickness));
            }
            auto pushPolygon = [&](const Primitive& primitive) {
                if (primitive.loop.size() < 2)
                    return;
                const auto points = particlePoints(primitive);
                if (points.size() != primitive.loop.size())
                    return;
                auto face = hullFace(points, shape, primitive.thickness);
                if (face.points.empty())
                    return;
                const auto key = sortedKnots(points);
                for (auto& existing : hull.faces) {
                    if (sortedKnots(existing.points) != key)
                        continue;
                    if (primitive.thickness > existing.thickness)
                        existing = std::move(face);
                    return;
                }
                hull.faces.push_back(std::move(face));
            };
            for (const auto& [_, primitive] : construction.membranes)
                pushPolygon(primitive);
            for (const auto& [_, primitive] : construction.plates)
                pushPolygon(primitive);
            for (const auto& [_, faces] : construction.volumes) {
                for (const auto& primitive : faces)
                    pushPolygon(primitive);
            }
            return phys::rigid::Crystal::Quantum{
                .particles = std::move(particles),
                .shape = std::move(shape),
                .com = mass > 0.0 ? vec3{moment / mass} : vec3{0.0f, 0.0f, 0.0f},
                .hull = std::move(hull),
            };
        }

    }

    auto Assembler::spawn(Writing context, scene::Root::Id root, Pose pose, Blueprint::Id blueprintId, vec3 velocity) -> Construct::Id {
        if (not with<Blueprint>::exists(context, blueprintId))
            return context.refuse("eltanin::mech::Assembler::spawn: blueprint missing");
        const auto interframe = with<resource::Assets>::find<resource::meshpack::Asset>(context, resource::Unit::Name::from("Eltanin", "interframe"));
        if (not interframe)
            return context.refuse("eltanin::mech::Assembler::spawn: interframe meshpack missing");

        const auto& blueprint = with<Blueprint>::get(context, blueprintId);
        auto [construction, fragments] = glueFrame(context, *interframe, blueprint);
        if (construction.knots.empty())
            return context.refuse("eltanin::mech::Assembler::spawn: blueprint has no knots");

        vector<Construction::Primitive::Id> visualOf;
        auto occurrences = cookOccurrences(context, *interframe, construction, fragments, visualOf);
        if (occurrences.empty())
            return context.refuse("eltanin::mech::Assembler::spawn: no quark meshes to compose");
        auto meshQuantum = with<scene::actor::Mesh>::compose(context, occurrences);
        if (not meshQuantum)
            return context.refuse("eltanin::mech::Assembler::spawn: mesh compose failed");
        auto meshState = with<scene::actor::MeshState>::defaults();
        meshState.latticeStep = space::local::edge2meters;
        const auto actor = with<scene::Interface>::createMeshActor(context, root, pose, std::move(*meshQuantum), std::move(meshState));

        auto crystal = crystalFrom(construction, pose, velocity);
        const auto body = with<phys::Body>::create(context, phys::rigid::restoredBody(pose, crystal.particles, crystal.shape));
        with<phys::rigid::Crystal>::extend(context, body, std::move(crystal));
        with<phys::Compound>::extend(context, body, phys::Compound::Quantum{.members = {}});

        const auto id = with<Construct>::create(context, Construct::Quantum{.body = body, .actor = actor, .fragments = std::move(fragments), .construction = std::move(construction), .visualOf = std::move(visualOf)});
        with<Construct>::syncVisualCohesion(context, id);
        return id;
    }

}
