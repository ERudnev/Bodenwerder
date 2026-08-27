#include "mech/assembler.h"

#include <eltanin/mech/mount.q1.h>
#include <eltanin/physics/compound.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>

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

        auto mountLoop(const space::Transform& transform, const Attachment& attachment) -> vector<index3> {
            vector<index3> loop;
            loop.reserve(attachment.points.size());
            for (const auto& point : attachment.points)
                loop.push_back(worldPoint(transform, point));
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

        auto glueFrame(Reading context, resource::meshpack::Asset::Id interframe, const Blueprint::Quantum& blueprint) -> std::pair<Construction, Construct::ActorFragments> {
            Construction construction{.knots = {}, .ribs = {}, .tiles = {}, .plates = {}, .weld4rib = {}};
            Construct::ActorFragments fragments{.ofKnot = {}, .ofRib = {}, .ofMembrane = {}, .ofPlate = {}};
            std::map<index3, Construction::Knot::Id, LatticeLess> knotsAt;
            std::map<EdgeKey, Construction::Rib::Id, EdgeLess> ribsAt;
            std::map<vector<index3>, Construction::Tile::Id, LoopLess> tilesAt;
            std::map<vector<index3>, Construction::Plate::Id, LoopLess> platesAt;

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
                        const auto id = static_cast<Construction::Knot::Id>(construction.knots.size());
                        construction.knots.emplace(id, Construction::Knot{.position = grid});
                        found = knotsAt.emplace(grid, id).first;
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
                        const auto id = static_cast<Construction::Rib::Id>(construction.ribs.size());
                        construction.ribs.emplace(id, Construction::Rib{.start = edge.first, .end = edge.second});
                        found = ribsAt.emplace(edge, id).first;
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
                    auto found = tilesAt.find(key);
                    if (found == tilesAt.end()) {
                        const auto id = static_cast<Construction::Tile::Id>(construction.tiles.size());
                        construction.tiles.emplace(id, Construction::Tile{.loop = std::move(loop)});
                        found = tilesAt.emplace(key, id).first;
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
                        .tile = found->second,
                        .quark = skeleton::Membrane{.kind = membrane.kind, .ori = world.ori},
                        .at = gridVertex(world, localSeat),
                    });
                }
            }

            constexpr std::size_t maxPlanarMountPoints = 4;
            for (const auto& placed : blueprint.mounts) {
                const auto mountId = with<resource::Assets>::find<Mount>(context, placed.mount);
                if (not mountId) {
                    base::message("eltanin::mech::Assembler::spawn: mount '{}' missing", placed.mount.text());
                    continue;
                }
                const auto& mount = with<Mount>::get(context, *mountId);
                if (not mount.attachment.flatMounted() or mount.collision.shape != Collision::Shape::capsule)
                    continue;
                if (mount.attachment.points.empty() or mount.attachment.points.size() > maxPlanarMountPoints)
                    continue;
                auto loop = mountLoop(placed.transform, mount.attachment);
                if (hasDuplicateVertex(loop))
                    continue;
                const auto key = cycleKey(loop);
                auto found = platesAt.find(key);
                if (found == platesAt.end()) {
                    const auto id = static_cast<Construction::Plate::Id>(construction.plates.size());
                    construction.plates.emplace(id, Construction::Plate{.loop = std::move(loop), .thickness = mount.collision.parameter1});
                    found = platesAt.emplace(key, id).first;
                } else {
                    auto& plate = construction.plates.at(found->second);
                    if (mount.collision.parameter1 > plate.thickness)
                        plate.thickness = mount.collision.parameter1;
                }
                fragments.ofPlate.push_back(Construct::ActorFragments::OfPlate{.plate = found->second, .mount = placed.mount, .transform = placed.transform});
            }

            for (const auto& [ribId, rib] : construction.ribs) {
                if (const auto knot = knotsAt.find(rib.start); knot != knotsAt.end())
                    construction.weld4rib.push_back(Construction::Weld4Rib{.knot = knot->second, .rib = ribId});
                if (not sameLattice(rib.start, rib.end)) {
                    if (const auto knot = knotsAt.find(rib.end); knot != knotsAt.end())
                        construction.weld4rib.push_back(Construction::Weld4Rib{.knot = knot->second, .rib = ribId});
                }
            }

            return {std::move(construction), std::move(fragments)};
        }

        auto cookOccurrences(Reading context, resource::meshpack::Asset::Id interframe, const Construction& construction, const Construct::ActorFragments& fragments) -> vector<scene::actor::Mesh::Occurrence> {
            vector<scene::actor::Mesh::Occurrence> occurrences;
            occurrences.reserve(fragments.ofKnot.size() + fragments.ofRib.size() + fragments.ofMembrane.size() + fragments.ofPlate.size());
            for (const auto& piece : fragments.ofKnot) {
                const auto resolved = with<resource::meshpack::Asset>::resolve(context, interframe, cornerMesh(piece.quark.kind));
                if (not resolved)
                    continue;
                const auto knot = construction.knots.find(piece.knot);
                if (knot == construction.knots.end())
                    continue;
                occurrences.push_back(scene::actor::Mesh::Occurrence{.entry = *resolved, .pose = renderer::DiscretePose{.pos = knot->second.position, .ori = piece.quark.ori}});
            }
            for (const auto& piece : fragments.ofRib) {
                const auto resolved = with<resource::meshpack::Asset>::resolve(context, interframe, halfribMesh(piece.quark.kind, piece.quark.pole));
                if (not resolved)
                    continue;
                occurrences.push_back(scene::actor::Mesh::Occurrence{.entry = *resolved, .pose = renderer::DiscretePose{.pos = piece.at, .ori = piece.quark.ori}});
            }
            for (const auto& piece : fragments.ofMembrane) {
                const auto resolved = with<resource::meshpack::Asset>::resolve(context, interframe, membraneMesh(piece.quark.kind));
                if (not resolved)
                    continue;
                occurrences.push_back(scene::actor::Mesh::Occurrence{.entry = *resolved, .pose = renderer::DiscretePose{.pos = piece.at, .ori = piece.quark.ori}});
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
            }
            return occurrences;
        }

        auto crystalFrom(const Construction& construction, Pose pose) -> phys::rigid::Crystal::Quantum {
            const auto count = construction.knots.size();
            vector<vec3> shape(count, vec3{0.0f, 0.0f, 0.0f});
            vector<phys::Particle> particles(count, phys::Particle{phys::Matter{.position = dvec3{0.0, 0.0, 0.0}, .mass = 1.0f, .temperature = 0.0f, .cohesion = 1.0f}, dvec3{0.0, 0.0, 0.0}, vec3{0.0f, 0.0f, 0.0f}});
            glm::dvec3 moment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (const auto& [id, knot] : construction.knots) {
                const auto index = static_cast<std::size_t>(id);
                const vec3 local{static_cast<float>(knot.position.x), static_cast<float>(knot.position.y), static_cast<float>(knot.position.z)};
                const vec3 meters = local * space::local::edge2meters;
                const vec3 world = pose.position + pose.rotation * meters;
                shape[index] = meters;
                particles[index] = phys::Particle{phys::Matter{.position = dvec3{world}, .mass = 1.0f, .temperature = 0.0f, .cohesion = 1.0f}, dvec3{world}, vec3{0.0f, 0.0f, 0.0f}};
                moment += glm::dvec3{meters};
                mass += 1.0;
            }
            std::map<index3, Construction::Knot::Id, LatticeLess> knotsAt;
            for (const auto& [id, knot] : construction.knots)
                knotsAt.emplace(knot.position, id);
            phys::rigid::Hull hull{.faces = {}};
            hull.faces.reserve(construction.ribs.size() + construction.tiles.size() + construction.plates.size());
            for (const auto& ribPair : construction.ribs) {
                const auto& rib = ribPair.second;
                const auto startKnot = knotsAt.find(rib.start);
                const auto endKnot = knotsAt.find(rib.end);
                if (startKnot == knotsAt.end() or endKnot == knotsAt.end() or startKnot->second == endKnot->second)
                    continue;
                hull.faces.push_back(beamFace(startKnot->second, endKnot->second, shape, 0.2f));
            }
            for (const auto& tilePair : construction.tiles) {
                vector<integer> points;
                points.reserve(tilePair.second.loop.size());
                bool complete = true;
                for (const auto& vertex : tilePair.second.loop) {
                    const auto knot = knotsAt.find(vertex);
                    if (knot == knotsAt.end()) {
                        complete = false;
                        break;
                    }
                    points.push_back(knot->second);
                }
                if (not complete)
                    continue;
                auto face = plateFace(points, shape, 0.2f);
                if (face.points.empty())
                    continue;
                hull.faces.push_back(std::move(face));
            }
            for (const auto& platePair : construction.plates) {
                const auto& plate = platePair.second;
                if (plate.loop.size() < 2)
                    continue;
                vector<integer> points;
                points.reserve(plate.loop.size());
                bool complete = true;
                for (const auto& vertex : plate.loop) {
                    const auto knot = knotsAt.find(vertex);
                    if (knot == knotsAt.end()) {
                        complete = false;
                        break;
                    }
                    points.push_back(knot->second);
                }
                if (not complete)
                    continue;
                auto face = hullFace(points, shape, plate.thickness);
                if (face.points.empty())
                    continue;
                const auto key = sortedKnots(points);
                bool replaced = false;
                for (auto& existing : hull.faces) {
                    if (sortedKnots(existing.points) != key)
                        continue;
                    if (plate.thickness > existing.thickness)
                        existing = std::move(face);
                    replaced = true;
                    break;
                }
                if (not replaced)
                    hull.faces.push_back(std::move(face));
            }
            return phys::rigid::Crystal::Quantum{
                .particles = std::move(particles),
                .shape = std::move(shape),
                .com = mass > 0.0 ? vec3{moment / mass} : vec3{0.0f, 0.0f, 0.0f},
                .hull = std::move(hull),
            };
        }

    }

    auto Assembler::spawn(Writing context, scene::Root::Id root, Pose pose, Blueprint::Id blueprintId) -> Construct::Id {
        if (not with<Blueprint>::exists(context, blueprintId))
            return context.refuse("eltanin::mech::Assembler::spawn: blueprint missing");
        const auto interframe = with<resource::Assets>::find<resource::meshpack::Asset>(context, resource::Unit::Name::from("Eltanin", "interframe"));
        if (not interframe)
            return context.refuse("eltanin::mech::Assembler::spawn: interframe meshpack missing");

        const auto& blueprint = with<Blueprint>::get(context, blueprintId);
        auto [construction, fragments] = glueFrame(context, *interframe, blueprint);
        if (construction.knots.empty())
            return context.refuse("eltanin::mech::Assembler::spawn: blueprint has no knots");

        auto occurrences = cookOccurrences(context, *interframe, construction, fragments);
        if (occurrences.empty())
            return context.refuse("eltanin::mech::Assembler::spawn: no quark meshes to compose");
        auto meshQuantum = with<scene::actor::Mesh>::compose(context, occurrences);
        if (not meshQuantum)
            return context.refuse("eltanin::mech::Assembler::spawn: mesh compose failed");
        auto meshState = with<scene::actor::MeshState>::defaults();
        meshState.latticeStep = space::local::edge2meters;
        const auto actor = with<scene::Interface>::createMeshActor(context, root, pose, std::move(*meshQuantum), std::move(meshState));

        auto crystal = crystalFrom(construction, pose);
        const auto body = with<phys::Body>::create(context, phys::rigid::restoredBody(pose, crystal.particles, crystal.shape));
        with<phys::rigid::Crystal>::extend(context, body, std::move(crystal));
        with<phys::Compound>::extend(context, body, phys::Compound::Quantum{.members = {}});

        return with<Construct>::create(context, Construct::Quantum{.body = body, .actor = actor, .fragments = std::move(fragments), .construction = std::move(construction)});
    }

}
