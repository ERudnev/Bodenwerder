#include "mech/assembler.h"

#include <eltanin/physics/compound.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>

#include "mech/semantics/space.h"
#include "mech/semantics/subframe.h"
#include "mech/semantics/quarks.h"

#include <base/logging.h>

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

        auto beamFace(integer start, integer end, const vector<vec3>& shape) -> phys::rigid::Hull::Face {
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
            return phys::rigid::Hull::Face{.points = {start, end}, .normal = normal, .thickness = 0.1f};
        }

        auto glueFrame(Reading context, resource::meshpack::Asset::Id interframe, const Blueprint::Quantum& blueprint) -> std::pair<Construction, Construct::ActorFragments> {
            Construction construction{.knots = {}, .ribs = {}, .weld4rib = {}};
            Construct::ActorFragments fragments{.ofKnot = {}, .ofRib = {}};
            std::map<index3, Construction::Knot::Id, LatticeLess> knotsAt;
            std::map<EdgeKey, Construction::Rib::Id, EdgeLess> ribsAt;

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
            occurrences.reserve(fragments.ofKnot.size() + fragments.ofRib.size());
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
            hull.faces.reserve(construction.ribs.size());
            for (const auto& ribPair : construction.ribs) {
                const auto& rib = ribPair.second;
                const auto startKnot = knotsAt.find(rib.start);
                const auto endKnot = knotsAt.find(rib.end);
                if (startKnot == knotsAt.end() or endKnot == knotsAt.end() or startKnot->second == endKnot->second)
                    continue;
                hull.faces.push_back(beamFace(startKnot->second, endKnot->second, shape));
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
