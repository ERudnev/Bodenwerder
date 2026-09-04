#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/scrap.q1.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/decorations/dust.q1.h>

#include "mech/assembler.h"
#include "mech/construction.h"
#include "mech/semantics/space.h"
#include "physics/hullBvh.h"
#include "physics/settings.h"
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <span>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>

namespace eltanin::locality {

    using namespace fqsm::api;

    namespace {

        struct PrimitiveHurt {
            float cohesion;
            float temperature;
        };

        auto hurtByPrimitive(const mech::Construction& construction, const vector<phys::Particle>& particles) -> umap<mech::Construction::Primitive::Id, PrimitiveHurt> {
            struct Acc {
                float cohesion;
                float temperature;
                std::size_t count;
            };
            umap<mech::Construction::Primitive::Id, Acc> acc;
            integer cursor = 0;
            mech::forEachPrimitiveLoop(construction, [&](mech::Construction::Primitive::Id id, const mech::Construction::Primitive& primitive) {
                const auto count = primitive.loop.size();
                for (std::size_t slot = 0; slot < count; ++slot) {
                    if (static_cast<std::size_t>(cursor) >= particles.size())
                        return;
                    const auto& particle = particles[static_cast<std::size_t>(cursor)];
                    ++cursor;
                    auto found = acc.find(id);
                    if (found == acc.end())
                        acc.emplace(id, Acc{.cohesion = particle.cohesion, .temperature = particle.temperature, .count = 1});
                    else {
                        found->second.cohesion += particle.cohesion;
                        found->second.temperature += particle.temperature;
                        ++found->second.count;
                    }
                }
            });
            umap<mech::Construction::Primitive::Id, PrimitiveHurt> hurt;
            for (const auto& [id, sum] : acc) {
                const float n = static_cast<float>(sum.count);
                hurt.emplace(id, PrimitiveHurt{.cohesion = sum.cohesion / n, .temperature = sum.temperature / n});
            }
            return hurt;
        }

        auto dropPrimitive(const umap<mech::Construction::Primitive::Id, PrimitiveHurt>& hurt, mech::Construction::Primitive::Id id) -> bool {
            const auto found = hurt.find(id);
            if (found == hurt.end())
                return false;
            return found->second.cohesion <= 0.0f or found->second.temperature >= phys::Settings::Heat::hullShedKelvin;
        }

        constexpr float minHalf = 0.25f;
        constexpr float heatUploadStep = 10.0f;
        constexpr float cohesionUploadStep = 0.001f;
        constexpr float membraneScrapThickness = 0.05f;
        constexpr float plateCollisionHalf = 0.05f; // 10 cm slab
        constexpr float platePlaneTrim = 0.10f; // 20 cm off each in-plane half-axis
        constexpr float ribEndTrim = 0.20f;
        constexpr float volumeFaceTrim = 0.50f;

        auto gpuStale(const vector<float>& was, const vector<float>& now, float step) -> bool {
            if (was.size() != now.size())
                return true;
            for (std::size_t index = 0; index < now.size(); ++index) {
                if (glm::abs(was[index] - now[index]) >= step)
                    return true;
            }
            return false;
        }

        void uploadVisual(Reading context, Construct::Quantum& construct) {
            if (not with<rmmr::scene::actor::Mesh>::exists(context, construct.actor))
                return;
            if (not with<phys::rigid::Crystal>::exists(context, construct.body))
                return;
            const auto& crystal = with<phys::rigid::Crystal>::get(context, construct.body);
            if (crystal.particles.size() != construct.construction.evaluatedParticles.size())
                return;
            const auto hurt = hurtByPrimitive(construct.construction, crystal.particles);
            vector<float> cohesions;
            vector<float> heats;
            cohesions.reserve(construct.visualOf.size());
            heats.reserve(construct.visualOf.size());
            for (const auto primitive : construct.visualOf) {
                const auto found = hurt.find(primitive);
                cohesions.push_back(found == hurt.end() ? 1.0f : found->second.cohesion);
                heats.push_back(found == hurt.end() ? 0.0f : found->second.temperature);
            }
            if (gpuStale(construct.gpuCohesions, cohesions, cohesionUploadStep)) {
                with<rmmr::scene::actor::Mesh>::writeCohesions(context, construct.actor, std::span<const float>{cohesions});
                construct.gpuCohesions = std::move(cohesions);
            }
            if (gpuStale(construct.gpuHeats, heats, heatUploadStep)) {
                with<rmmr::scene::actor::Mesh>::writeHeats(context, construct.actor, std::span<const float>{heats});
                construct.gpuHeats = std::move(heats);
            }
        }

        auto quatFromAxes(vec3 x, vec3 y, vec3 z) -> quat {
            return glm::normalize(glm::quat_cast(mat3{x, y, z}));
        }

        auto perpendicular(vec3 axis) -> vec3 {
            vec3 hint{0.0f, 1.0f, 0.0f};
            if (glm::abs(glm::dot(axis, hint)) > 0.9f)
                hint = vec3{1.0f, 0.0f, 0.0f};
            return glm::normalize(glm::cross(axis, hint));
        }

        struct LocalBox {
            vec3 center;
            quat rotation;
            vec3 half;
        };

        auto boxOf(const vector<vec3>& points, float shell) -> LocalBox {
            const quat identity{1.0f, 0.0f, 0.0f, 0.0f};
            if (points.empty())
                return LocalBox{.center = vec3{0.0f, 0.0f, 0.0f}, .rotation = identity, .half = vec3{minHalf, minHalf, minHalf}};
            if (points.size() == 1)
                return LocalBox{.center = points[0], .rotation = identity, .half = vec3{shell, shell, shell}};
            if (points.size() == 2) {
                const vec3 delta = points[1] - points[0];
                const float length = glm::length(delta);
                if (length < 1.0e-5f)
                    return LocalBox{.center = points[0], .rotation = identity, .half = vec3{shell, shell, shell}};
                const vec3 x = delta / length;
                const vec3 y = perpendicular(x);
                const vec3 z = glm::cross(x, y);
                return LocalBox{.center = 0.5f * (points[0] + points[1]), .rotation = quatFromAxes(x, y, z), .half = vec3{length * 0.5f, shell, shell}};
            }
            vec3 sum{0.0f, 0.0f, 0.0f};
            for (const vec3& point : points)
                sum += point;
            const vec3 centroid = sum / static_cast<float>(points.size());
            vec3 normal = glm::cross(points[1] - points[0], points[2] - points[0]);
            const float normalLen = glm::length(normal);
            if (normalLen < 1.0e-8f)
                return LocalBox{.center = centroid, .rotation = identity, .half = vec3{shell, shell, shell}};
            normal /= normalLen;
            const vec3 x = glm::normalize(points[1] - points[0]);
            const vec3 y = glm::cross(normal, x);
            float minX = 1.0e30f;
            float maxX = -1.0e30f;
            float minY = 1.0e30f;
            float maxY = -1.0e30f;
            for (const vec3& point : points) {
                const vec3 delta = point - centroid;
                const float u = glm::dot(delta, x);
                const float v = glm::dot(delta, y);
                minX = glm::min(minX, u);
                maxX = glm::max(maxX, u);
                minY = glm::min(minY, v);
                maxY = glm::max(maxY, v);
            }
            const vec3 center = centroid + x * (0.5f * (minX + maxX)) + y * (0.5f * (minY + maxY));
            return LocalBox{.center = center, .rotation = quatFromAxes(x, y, normal), .half = vec3{glm::max(0.5f * (maxX - minX), minHalf), glm::max(0.5f * (maxY - minY), minHalf), shell}};
        }

        auto boxOfRib(const vector<vec3>& points, float shell) -> LocalBox {
            auto box = boxOf(points, shell);
            const float span = glm::max(box.half.x * 2.0f - 2.0f * ribEndTrim, minHalf * 2.0f);
            box.half.x = span * 0.5f;
            return box;
        }

        auto boxOfVolume(const vector<vec3>& points) -> LocalBox {
            const quat identity{1.0f, 0.0f, 0.0f, 0.0f};
            if (points.empty())
                return LocalBox{.center = vec3{0.0f, 0.0f, 0.0f}, .rotation = identity, .half = vec3{minHalf, minHalf, minHalf}};
            vec3 boundMin = points[0];
            vec3 boundMax = points[0];
            for (const vec3& point : points) {
                boundMin = glm::min(boundMin, point);
                boundMax = glm::max(boundMax, point);
            }
            const vec3 half = glm::max(0.5f * (boundMax - boundMin) - vec3{volumeFaceTrim, volumeFaceTrim, volumeFaceTrim}, vec3{minHalf, minHalf, minHalf});
            return LocalBox{.center = 0.5f * (boundMin + boundMax), .rotation = identity, .half = half};
        }

        auto sameGrid(const index3& left, const index3& right) -> bool {
            return left.x == right.x and left.y == right.y and left.z == right.z;
        }

        auto hasKnotAt(const mech::Construction& construction, index3 grid) -> bool {
            for (const auto& [_, knot] : construction.knots) {
                if (not knot.loop.empty() and sameGrid(knot.loop[0].gridPos, grid))
                    return true;
            }
            return false;
        }

        auto knotCountInCell(const mech::Construction& construction, index3 cell) -> int {
            int count = 0;
            for (int dx = 0; dx < 2; ++dx)
                for (int dy = 0; dy < 2; ++dy)
                    for (int dz = 0; dz < 2; ++dz)
                        if (hasKnotAt(construction, index3{.x = cell.x + dx, .y = cell.y + dy, .z = cell.z + dz}))
                            ++count;
            return count;
        }

        auto cellOf(vec3 gridPoint) -> index3 {
            const vec3 floored = glm::floor(gridPoint);
            return index3{.x = static_cast<int>(floored.x), .y = static_cast<int>(floored.y), .z = static_cast<int>(floored.z)};
        }

        // Plate is glued to the outside of a cell. Occupied cell = more knots; outward goes through the face from that cell. Tie with knots on both sides → no shift. No skeleton → winding (CCW-out).
        auto plateOutward(const mech::Construction::Primitive& plate, const mech::Construction& construction) -> vec3 {
            if (plate.loop.size() < 3)
                return vec3{0.0f, 0.0f, 0.0f};
            vec3 sum{0.0f, 0.0f, 0.0f};
            for (const auto& welded : plate.loop)
                sum += vec3{static_cast<float>(welded.gridPos.x), static_cast<float>(welded.gridPos.y), static_cast<float>(welded.gridPos.z)};
            const vec3 centroid = sum / static_cast<float>(plate.loop.size());
            const vec3 ab = vec3{static_cast<float>(plate.loop[1].gridPos.x - plate.loop[0].gridPos.x), static_cast<float>(plate.loop[1].gridPos.y - plate.loop[0].gridPos.y), static_cast<float>(plate.loop[1].gridPos.z - plate.loop[0].gridPos.z)};
            const vec3 ac = vec3{static_cast<float>(plate.loop[2].gridPos.x - plate.loop[0].gridPos.x), static_cast<float>(plate.loop[2].gridPos.y - plate.loop[0].gridPos.y), static_cast<float>(plate.loop[2].gridPos.z - plate.loop[0].gridPos.z)};
            vec3 normal = glm::cross(ab, ac);
            const float normalLen = glm::length(normal);
            if (normalLen < 1.0e-8f)
                return vec3{0.0f, 0.0f, 0.0f};
            normal /= normalLen;
            const index3 plusCell = cellOf(centroid + 0.5f * normal);
            const index3 minusCell = cellOf(centroid - 0.5f * normal);
            if (sameGrid(plusCell, minusCell))
                return normal;
            const int plusKnots = knotCountInCell(construction, plusCell);
            const int minusKnots = knotCountInCell(construction, minusCell);
            if (plusKnots == minusKnots)
                return plusKnots > 0 ? vec3{0.0f, 0.0f, 0.0f} : normal;
            return plusKnots > minusKnots ? -normal : normal;
        }

        auto scrapBox(const mech::Construction& construction, mech::Construction::Primitive::Id id, const vector<vec3>& locals, float thickness, vec3 outward) -> LocalBox {
            if (construction.volumes.contains(id))
                return boxOfVolume(locals);
            if (construction.ribs.contains(id))
                return boxOfRib(locals, glm::max(thickness * 0.5f, 0.08f));
            if (construction.membranes.contains(id))
                return boxOf(locals, membraneScrapThickness * 0.5f);
            if (construction.plates.contains(id)) {
                auto box = boxOf(locals, plateCollisionHalf);
                box.half.x = glm::max(box.half.x - platePlaneTrim, minHalf);
                box.half.y = glm::max(box.half.y - platePlaneTrim, minHalf);
                if (glm::length(outward) > 1.0e-8f)
                    box.center += outward * plateCollisionHalf;
                return box;
            }
            return boxOf(locals, glm::max(thickness * 0.5f, 0.08f));
        }

        struct DeadChunk {
            float cohesion;
            float temperature;
            float thickness;
            float mass;
            dvec3 momentum;
            vector<vec3> locals;
            vec3 outward;
        };

        template<typename Piece, typename IdOf>
        void keepLive(vector<Piece>& items, IdOf idOf, const umap<mech::Construction::Primitive::Id, PrimitiveHurt>& hurt) {
            vector<Piece> live;
            live.reserve(items.size());
            for (auto& piece : items) {
                if (not dropPrimitive(hurt, idOf(piece)))
                    live.push_back(std::move(piece));
            }
            items = std::move(live);
        }

        auto fragmentsOf(const Construct::ActorFragments& fragments, mech::Construction::Primitive::Id id) -> Construct::ActorFragments {
            Construct::ActorFragments slice{.ofKnot = {}, .ofRib = {}, .ofMembrane = {}, .ofPlate = {}, .ofVolume = {}};
            for (const auto& piece : fragments.ofKnot)
                if (piece.knot == id)
                    slice.ofKnot.push_back(piece);
            for (const auto& piece : fragments.ofRib)
                if (piece.rib == id)
                    slice.ofRib.push_back(piece);
            for (const auto& piece : fragments.ofMembrane)
                if (piece.membrane == id)
                    slice.ofMembrane.push_back(piece);
            for (const auto& piece : fragments.ofPlate)
                if (piece.plate == id)
                    slice.ofPlate.push_back(piece);
            for (const auto& piece : fragments.ofVolume)
                if (piece.volume == id)
                    slice.ofVolume.push_back(piece);
            return slice;
        }

        auto asKeep(const vector<mech::Construction::Primitive::Id>& ids) -> umap<mech::Construction::Primitive::Id, bool> {
            umap<mech::Construction::Primitive::Id, bool> keep;
            for (const auto id : ids)
                keep.emplace(id, true);
            return keep;
        }

        auto takeConstruction(const mech::Construction& src, const umap<mech::Construction::Primitive::Id, bool>& keep) -> mech::Construction {
            mech::Construction slice{.knots = {}, .ribs = {}, .membranes = {}, .plates = {}, .volumes = {}, .evaluatedParticles = {}};
            auto copyKept = [&](const auto& from, auto& to) {
                for (const auto& [id, primitive] : from) {
                    if (keep.contains(id))
                        to.emplace(id, primitive);
                }
            };
            copyKept(src.knots, slice.knots);
            copyKept(src.ribs, slice.ribs);
            copyKept(src.membranes, slice.membranes);
            copyKept(src.plates, slice.plates);
            copyKept(src.volumes, slice.volumes);
            mech::compileParticles(slice);
            return slice;
        }

        auto takeFragments(const Construct::ActorFragments& fragments, const umap<mech::Construction::Primitive::Id, bool>& keep) -> Construct::ActorFragments {
            Construct::ActorFragments slice{.ofKnot = {}, .ofRib = {}, .ofMembrane = {}, .ofPlate = {}, .ofVolume = {}};
            auto copyKept = [&](const auto& from, auto& to, auto idOf) {
                for (const auto& piece : from) {
                    if (keep.contains(idOf(piece)))
                        to.push_back(piece);
                }
            };
            copyKept(fragments.ofKnot, slice.ofKnot, [](const auto& piece) { return piece.knot; });
            copyKept(fragments.ofRib, slice.ofRib, [](const auto& piece) { return piece.rib; });
            copyKept(fragments.ofMembrane, slice.ofMembrane, [](const auto& piece) { return piece.membrane; });
            copyKept(fragments.ofPlate, slice.ofPlate, [](const auto& piece) { return piece.plate; });
            copyKept(fragments.ofVolume, slice.ofVolume, [](const auto& piece) { return piece.volume; });
            return slice;
        }

        template<typename Piece, typename IdOf>
        void keepMarked(vector<Piece>& items, IdOf idOf, const umap<mech::Construction::Primitive::Id, bool>& keep) {
            vector<Piece> live;
            live.reserve(items.size());
            for (auto& piece : items) {
                if (keep.contains(idOf(piece)))
                    live.push_back(std::move(piece));
            }
            items = std::move(live);
        }

        void emitScrap(Writing context, const mech::Construction& construction, const Construct::ActorFragments& fragments, const phys::Body::Quantum& body, mech::Construction::Primitive::Id primitiveId, const DeadChunk& chunk) {
            if (chunk.mass <= 0.0f or chunk.locals.empty())
                return;
            const auto box = scrapBox(construction, primitiveId, chunk.locals, chunk.thickness, chunk.outward);
            const vec3 worldCenter = vec3{body.position} + body.orientation * box.center;
            const quat worldRot = glm::normalize(body.orientation * box.rotation);
            const vec3 linear = vec3{chunk.momentum / double(chunk.mass)};
            if (chunk.temperature < phys::Settings::Heat::hullShedKelvin) {
                const int cuts = Scrap::Actions::cutCount(chunk.cohesion);
                if (cuts < 0)
                    return;
                const bool volume = construction.volumes.contains(primitiveId);
                base::maybe<phys::Body::Id> cohort{};
                if (phys::Settings::debrisCohort == phys::Settings::DebrisCohort::unified)
                    cohort = body.compound;
                if (volume or cuts == 0) {
                    const auto& constructResources = with<Construct>::get_global(context).resources;
                    vector<mech::Construction::Primitive::Id> visualOf;
                    auto occurrences = constructResources ? mech::cookOccurrences(context, constructResources->interframe, construction, fragmentsOf(fragments, primitiveId), visualOf) : vector<rmmr::scene::actor::Mesh::Occurrence>{};
                    const rmmr::Pose bodyPose{.position = worldCenter, .rotation = worldRot};
                    const auto lineage = volume ? Scrap::Lineage::volume : Scrap::Lineage::common;
                    Scrap::Actions::spawnMesh(context, body.pose(), bodyPose, box.half, chunk.mass, linear, vec3{0.0f, 0.0f, 0.0f}, chunk.cohesion, chunk.temperature, std::move(occurrences), mech::space::local::edge2meters, lineage, cohort);
                    return;
                }
                Scrap::Actions::breakOff(context, worldCenter, worldRot, box.half, chunk.mass, linear, chunk.cohesion, chunk.temperature, cohort);
                return;
            }
            const auto& constructResources = with<Construct>::get_global(context).resources;
            vector<mech::Construction::Primitive::Id> visualOf;
            auto occurrences = constructResources ? mech::cookOccurrences(context, constructResources->interframe, construction, fragmentsOf(fragments, primitiveId), visualOf) : vector<rmmr::scene::actor::Mesh::Occurrence>{};
            if (not occurrences.empty())
                decorations::Dust::Actions::spawnMesh(context, body.pose(), std::move(occurrences), linear, vec3{0.0f, 0.0f, 0.0f}, chunk.temperature, box.half, mech::space::local::edge2meters);
            else
                decorations::Dust::Actions::spawn(context, rmmr::Pose{.position = worldCenter, .rotation = worldRot}, box.half, linear, vec3{0.0f, 0.0f, 0.0f}, chunk.temperature);
        }

        auto budConstruct(Writing context, const phys::Body::Quantum& body, mech::Construction slice, Construct::ActorFragments fragments, vector<phys::Particle> particles, vector<vec3> shape) -> bool {
            if (particles.empty() or particles.size() != slice.evaluatedParticles.size() or particles.size() != shape.size())
                return false;
            const auto& resources = with<Construct>::get_global(context).resources;
            if (not resources)
                return false;
            vector<mech::Construction::Primitive::Id> visualOf;
            auto occurrences = mech::cookOccurrences(context, resources->interframe, slice, fragments, visualOf);
            if (occurrences.empty())
                return false;
            auto meshQuantum = with<rmmr::scene::actor::Mesh>::compose(context, occurrences);
            if (not meshQuantum)
                return false;
            auto meshState = with<rmmr::scene::actor::MeshState>::defaults();
            meshState.latticeStep = mech::space::local::edge2meters;
            const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, with<Thing>::get_global(context).scene, body.pose(), std::move(*meshQuantum), std::move(meshState));
            glm::dvec3 moment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (std::size_t index = 0; index < particles.size(); ++index) {
                moment += glm::dvec3{shape[index]} * static_cast<double>(particles[index].mass);
                mass += static_cast<double>(particles[index].mass);
            }
            auto hull = mech::cookHull(slice, shape);
            phys::rigid::Crystal::Quantum crystal{.particles = std::move(particles), .shape = std::move(shape), .com = mass > 0.0 ? vec3{moment / mass} : vec3{0.0f, 0.0f, 0.0f}, .hull = std::move(hull)};
            phys::collision::cookHullBvh(crystal.hull, crystal.shape);
            const auto crystalBody = phys::createBody(context, phys::rigid::restoredBody(body.pose(), crystal.particles, crystal.shape), {});
            with<phys::rigid::Crystal>::extend(context, crystalBody, std::move(crystal));
            const auto thing = with<Thing>::create(context, Thing::Quantum{.bornAt = with<Thing>::get_global(context).now});
            with<Construct>::extend(context, thing, Construct::Quantum{.body = crystalBody, .actor = actor, .fragments = std::move(fragments), .construction = std::move(slice), .visualOf = std::move(visualOf), .gpuCohesions = {}, .gpuHeats = {}});
            Construct::Actions::syncVisualCohesion(context, thing);
            return true;
        }

        auto detachUnconnected(Writing context, Construct::Id id) -> bool {
            if (not with<Construct>::exists(context, id))
                return true;
            auto construct = with<Construct>::modify(context, id);
            if (not with<phys::rigid::Crystal>::exists(context, construct->body))
                return true;
            auto crystal = with<phys::rigid::Crystal>::modify(context, construct->body);
            if (crystal->particles.size() != construct->construction.evaluatedParticles.size() or crystal->particles.size() != crystal->shape.size())
                return true;
            const auto frame = mech::connectedIslands(construct->construction);
            const auto& islands = frame.islands;
            if (islands.empty() and frame.shedSkin.empty())
                return false;
            if (islands.size() == 1 and frame.shedSkin.empty() and mech::islandIsConstruct(construct->construction, islands[0]))
                return true;
            if (not with<phys::Body>::exists(context, construct->body))
                return true;
            const auto construction = construct->construction;
            const auto fragments = construct->fragments;
            const auto body = with<phys::Body>::get(context, construct->body);
            const auto hurt = hurtByPrimitive(construction, crystal->particles);

            umap<mech::Construction::Primitive::Id, std::size_t> islandOf;
            for (std::size_t index = 0; index < islands.size(); ++index) {
                for (const auto id : islands[index])
                    islandOf.emplace(id, index);
            }
            const auto shedOf = asKeep(frame.shedSkin);
            struct IslandMatter {
                vector<phys::Particle> particles;
                vector<vec3> shape;
            };
            vector<IslandMatter> matter;
            matter.reserve(islands.size());
            for (std::size_t index = 0; index < islands.size(); ++index)
                matter.push_back(IslandMatter{.particles = {}, .shape = {}});
            umap<mech::Construction::Primitive::Id, DeadChunk> pieces;
            integer cursor = 0;
            bool mismatch = false;
            mech::forEachPrimitiveLoop(construction, [&](mech::Construction::Primitive::Id primitiveId, const mech::Construction::Primitive& primitive) {
                if (mismatch)
                    return;
                const auto islandFound = islandOf.find(primitiveId);
                const bool shed = shedOf.contains(primitiveId);
                if (islandFound == islandOf.end() and not shed) {
                    mismatch = true;
                    return;
                }
                auto found = pieces.find(primitiveId);
                if (found == pieces.end()) {
                    const auto state = hurt.find(primitiveId);
                    const vec3 outward = construction.plates.contains(primitiveId) ? plateOutward(primitive, construction) : vec3{0.0f, 0.0f, 0.0f};
                    const float cohesion = state == hurt.end() ? 0.0f : state->second.cohesion;
                    const float temperature = state == hurt.end() ? 0.0f : state->second.temperature;
                    found = pieces.emplace(primitiveId, DeadChunk{.cohesion = cohesion, .temperature = temperature, .thickness = primitive.thickness, .mass = 0.0f, .momentum = dvec3{0.0, 0.0, 0.0}, .locals = {}, .outward = outward}).first;
                }
                found->second.thickness = glm::max(found->second.thickness, primitive.thickness);
                auto& chunk = found->second;
                IslandMatter* bin = islandFound == islandOf.end() ? nullptr : &matter[islandFound->second];
                for (std::size_t slot = 0; slot < primitive.loop.size(); ++slot) {
                    const auto index = static_cast<std::size_t>(cursor);
                    if (index >= crystal->particles.size() or index >= crystal->shape.size()) {
                        mismatch = true;
                        return;
                    }
                    const auto& particle = crystal->particles[index];
                    if (bin) {
                        bin->particles.push_back(particle);
                        bin->shape.push_back(crystal->shape[index]);
                    }
                    chunk.locals.push_back(crystal->shape[index]);
                    chunk.mass += particle.mass;
                    chunk.momentum += (particle.position - particle.prev) / phys::Settings::fixedStep * double(particle.mass);
                    ++cursor;
                }
            });
            if (mismatch or static_cast<std::size_t>(cursor) != crystal->particles.size())
                return true;

            std::size_t keeper = islands.size();
            double bestMass = -1.0;
            std::size_t bestCount = 0;
            for (std::size_t index = 0; index < islands.size(); ++index) {
                if (not mech::islandIsConstruct(construction, islands[index]))
                    continue;
                double mass = 0.0;
                for (const auto& particle : matter[index].particles)
                    mass += static_cast<double>(particle.mass);
                const auto count = islands[index].size();
                if (mass > bestMass or (mass == bestMass and count > bestCount)) {
                    bestMass = mass;
                    bestCount = count;
                    keeper = index;
                }
            }

            auto scrapIds = [&](const vector<mech::Construction::Primitive::Id>& ids) {
                for (const auto primitiveId : ids) {
                    auto found = pieces.find(primitiveId);
                    if (found == pieces.end())
                        continue;
                    emitScrap(context, construction, fragments, body, primitiveId, found->second);
                }
            };
            auto scrapIsland = [&](std::size_t index) {
                scrapIds(islands[index]);
            };
            scrapIds(frame.shedSkin);
            for (std::size_t index = 0; index < islands.size(); ++index) {
                if (index == keeper)
                    continue;
                if (mech::islandIsConstruct(construction, islands[index])) {
                    const auto keep = asKeep(islands[index]);
                    if (not budConstruct(context, body, takeConstruction(construction, keep), takeFragments(fragments, keep), std::move(matter[index].particles), std::move(matter[index].shape)))
                        scrapIsland(index);
                    continue;
                }
                scrapIsland(index);
            }
            if (keeper >= islands.size())
                return false;

            const auto keep = asKeep(islands[keeper]);
            auto eraseUnkept = [&](auto& items) {
                for (auto it = items.begin(); it != items.end(); ) {
                    if (keep.contains(it->first))
                        ++it;
                    else
                        it = items.erase(it);
                }
            };
            eraseUnkept(construct->construction.knots);
            eraseUnkept(construct->construction.ribs);
            eraseUnkept(construct->construction.membranes);
            eraseUnkept(construct->construction.plates);
            eraseUnkept(construct->construction.volumes);
            keepMarked(construct->fragments.ofKnot, [](const auto& piece) { return piece.knot; }, keep);
            keepMarked(construct->fragments.ofRib, [](const auto& piece) { return piece.rib; }, keep);
            keepMarked(construct->fragments.ofMembrane, [](const auto& piece) { return piece.membrane; }, keep);
            keepMarked(construct->fragments.ofPlate, [](const auto& piece) { return piece.plate; }, keep);
            keepMarked(construct->fragments.ofVolume, [](const auto& piece) { return piece.volume; }, keep);
            mech::compileParticles(construct->construction);
            crystal->particles = std::move(matter[keeper].particles);
            crystal->shape = std::move(matter[keeper].shape);
            glm::dvec3 moment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (std::size_t index = 0; index < crystal->particles.size(); ++index) {
                moment += glm::dvec3{crystal->shape[index]} * static_cast<double>(crystal->particles[index].mass);
                mass += static_cast<double>(crystal->particles[index].mass);
            }
            crystal->com = mass > 0.0 ? vec3{moment / mass} : vec3{0.0f, 0.0f, 0.0f};
            return not crystal->particles.empty();
        }

        auto shedOne(Writing context, Construct::Id id) -> bool {
            if (not with<Construct>::exists(context, id))
                return true;
            auto construct = with<Construct>::modify(context, id);
            if (not with<phys::rigid::Crystal>::exists(context, construct->body) or not with<rmmr::scene::actor::Mesh>::exists(context, construct->actor))
                return true;
            auto crystal = with<phys::rigid::Crystal>::modify(context, construct->body);
            if (crystal->particles.size() != construct->construction.evaluatedParticles.size() or crystal->particles.size() != crystal->shape.size())
                return true;
            const auto hurt = hurtByPrimitive(construct->construction, crystal->particles);
            bool anyDead = false;
            for (const auto& [_, state] : hurt) {
                if (state.cohesion <= 0.0f or state.temperature >= phys::Settings::Heat::hullShedKelvin) {
                    anyDead = true;
                    break;
                }
            }
            if (not anyDead)
                return true;

            vector<phys::Particle> particles;
            vector<vec3> shape;
            particles.reserve(crystal->particles.size());
            shape.reserve(crystal->shape.size());
            umap<mech::Construction::Primitive::Id, DeadChunk> dead;
            integer cursor = 0;
            bool mismatch = false;
            mech::forEachPrimitiveLoop(construct->construction, [&](mech::Construction::Primitive::Id primitiveId, const mech::Construction::Primitive& primitive) {
                if (mismatch)
                    return;
                const bool gone = dropPrimitive(hurt, primitiveId);
                DeadChunk* chunk = nullptr;
                if (gone) {
                    auto found = dead.find(primitiveId);
                    if (found == dead.end()) {
                        const auto state = hurt.find(primitiveId);
                        const vec3 outward = construct->construction.plates.contains(primitiveId) ? plateOutward(primitive, construct->construction) : vec3{0.0f, 0.0f, 0.0f};
                        found = dead.emplace(primitiveId, DeadChunk{.cohesion = state->second.cohesion, .temperature = state->second.temperature, .thickness = primitive.thickness, .mass = 0.0f, .momentum = dvec3{0.0, 0.0, 0.0}, .locals = {}, .outward = outward}).first;
                    }
                    found->second.thickness = glm::max(found->second.thickness, primitive.thickness);
                    chunk = &found->second;
                }
                for (std::size_t slot = 0; slot < primitive.loop.size(); ++slot) {
                    const auto index = static_cast<std::size_t>(cursor);
                    if (index >= crystal->particles.size() or index >= crystal->shape.size()) {
                        mismatch = true;
                        return;
                    }
                    if (gone) {
                        const auto& particle = crystal->particles[index];
                        chunk->locals.push_back(crystal->shape[index]);
                        chunk->mass += particle.mass;
                        chunk->momentum += (particle.position - particle.prev) / phys::Settings::fixedStep * double(particle.mass);
                    } else {
                        particles.push_back(crystal->particles[index]);
                        shape.push_back(crystal->shape[index]);
                    }
                    ++cursor;
                }
            });
            if (mismatch or static_cast<std::size_t>(cursor) != crystal->particles.size())
                return true;

            if (with<phys::Body>::exists(context, construct->body)) {
                const auto& body = with<phys::Body>::get(context, construct->body);
                for (const auto& [primitiveId, chunk] : dead)
                    emitScrap(context, construct->construction, construct->fragments, body, primitiveId, chunk);
            }

            if (particles.empty())
                return false;

            glm::dvec3 moment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (std::size_t index = 0; index < particles.size(); ++index) {
                moment += glm::dvec3{shape[index]} * static_cast<double>(particles[index].mass);
                mass += static_cast<double>(particles[index].mass);
            }
            auto eraseDead = [&](auto& items) {
                for (auto it = items.begin(); it != items.end(); ) {
                    if (dropPrimitive(hurt, it->first))
                        it = items.erase(it);
                    else
                        ++it;
                }
            };
            eraseDead(construct->construction.knots);
            eraseDead(construct->construction.ribs);
            eraseDead(construct->construction.membranes);
            eraseDead(construct->construction.plates);
            eraseDead(construct->construction.volumes);
            keepLive(construct->fragments.ofKnot, [](const auto& piece) { return piece.knot; }, hurt);
            keepLive(construct->fragments.ofRib, [](const auto& piece) { return piece.rib; }, hurt);
            keepLive(construct->fragments.ofMembrane, [](const auto& piece) { return piece.membrane; }, hurt);
            keepLive(construct->fragments.ofPlate, [](const auto& piece) { return piece.plate; }, hurt);
            keepLive(construct->fragments.ofVolume, [](const auto& piece) { return piece.volume; }, hurt);
            mech::compileParticles(construct->construction);
            crystal->particles = std::move(particles);
            crystal->shape = std::move(shape);
            crystal->com = mass > 0.0 ? vec3{moment / mass} : vec3{0.0f, 0.0f, 0.0f};
            if (not detachUnconnected(context, id))
                return false;
            crystal->hull = mech::cookHull(construct->construction, crystal->shape);
            phys::collision::cookHullBvh(crystal->hull, crystal->shape);
            if (with<phys::Body>::exists(context, construct->body))
                crystal->refreshMatter(*with<phys::Body>::modify(context, construct->body));

            const auto& resources = with<Construct>::get_global(context).resources;
            if (not resources)
                return context.refuse("eltanin::locality::Construct::shedDead: resources not bound");
            auto occurrences = mech::cookOccurrences(context, resources->interframe, construct->construction, construct->fragments, construct->visualOf);
            if (not occurrences.empty()) {
                auto meshQuantum = with<rmmr::scene::actor::Mesh>::compose(context, occurrences);
                if (meshQuantum)
                    with<rmmr::scene::actor::Mesh>::replace(context, construct->actor, std::move(*meshQuantum));
            }
            construct->gpuCohesions.clear();
            construct->gpuHeats.clear();
            uploadVisual(context, *construct);
            return true;
        }

    }

    void Construct::Actions::bindResources(Writing context) {
        if (with<Construct>::get_global(context).resources)
            return;
        const auto interframe = with<rmmr::resource::Assets>::find<rmmr::resource::meshpack::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "interframe"));
        if (not interframe) {
            context.refuse("eltanin::locality::Construct::bindResources: interframe meshpack missing");
            return;
        }
        with<Construct>::modify_global(context)->resources = Resources{.interframe = *interframe};
    }

    void Construct::Actions::update(Writing context) {
        shedDead(context);
    }

    void Construct::Actions::shedDead(Writing context) {
        vector<Id> gone;
        vector<Id> living;
        for (auto [id, _] : context->aspect<Construct>().items())
            living.push_back(id);
        for (const auto id : living) {
            if (not shedOne(context, id))
                gone.push_back(id);
        }
        for (const auto id : gone)
            with<Construct>::kraken(context, id);
    }

    void Construct::Actions::radiate(Stewarding context, seconds dt) {
        if (dt <= 0)
            return;
        const float sky = with<rmmr::scene::Root>::get(context, with<Thing>::get_global(context).scene).atmosphereTemperature;
        const float remaining = glm::max(0.0f, 1.0f - float(dt) / float(phys::Settings::Heat::hullCool));
        const float factor = remaining * remaining;
        auto crystals = context.direct<phys::rigid::Crystal>();
        for (auto [_, construct] : context.direct<Construct>().items) {
            auto* crystal = crystals.items.find(construct.body);
            if (not crystal)
                continue;
            for (phys::Particle& particle : crystal->particles) {
                if (particle.temperature <= sky)
                    continue;
                particle.temperature = glm::max(sky, particle.temperature * factor);
            }
        }
    }

    void Construct::Actions::followBody(Stewarding context) {
        auto nodes = context.direct<rmmr::scene::Node>();
        auto bodies = context.direct<phys::Body>();
        auto constructs = context.direct<Construct>();
        for (auto [id, construct] : constructs.items) {
            auto* node = nodes.items.find(construct.actor);
            if (not node)
                continue;
            auto* body = bodies.items.find(construct.body);
            if (not body)
                continue;
            node->pose = body->pose();
            uploadVisual(context, construct);
        }
    }

    void Construct::Actions::syncVisualCohesion(Writing context, Id id) {
        uploadVisual(context, *with<Construct>::modify(context, id));
    }

    auto Construct::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Construct, rmmr::scene::actor::Mesh, &Construct::Quantum::actor>{},
            reaction::structural::custody<Construct, phys::rigid::Crystal, &Construct::Quantum::body>{},
        };
    }

}
