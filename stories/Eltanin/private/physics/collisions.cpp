#include "physics/collisions.h"
#include "physics/hullBvh.h"
#include "physics/system.h"

#include <base/logging.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace eltanin::phys::collision {

    using rigid::Solid;
    using rigid::Crystal;
    using rigid::Ray;

    namespace {

        constexpr float minLength = 1.0e-8f;
        constexpr float solidFriction = 0.8f;
        constexpr float solidRestitution = 0.6f; // rigid primitives only; Crystal particles stay e≈0 positional kicks
        constexpr float rayRestitution = 0.45f;
        constexpr float rayPierceBegin = 200.0f;
        constexpr float rayPierceFull = 800.0f;
        constexpr float rayPierceKeep = 0.70f;
        constexpr float raySkin = 1.0e-4f;

        struct Occupant {
            Endpoint::Type type;
            Body::Id body;
            vec3 center;
            float radius;
        };

        auto isSimple(Endpoint::Type type) -> bool {
            return type == Endpoint::Type::sphere or type == Endpoint::Type::box;
        }

        auto halfOf(const Solid::Quantum& solid, const Body::Quantum& body) -> vec3 {
            if (solid.kind == Solid::Kind::box)
                return glm::max(solid.halfExtents, vec3{minLength, minLength, minLength});
            return vec3{body.radius, body.radius, body.radius};
        }

        auto toBodyLocal(const Body::Quantum& body, vec3 world) -> vec3 {
            return glm::conjugate(body.orientation) * (world - vec3{body.position});
        }

        auto fromBodyLocal(const Body::Quantum& body, vec3 local) -> vec3 {
            return vec3{body.position} + body.orientation * local;
        }

        auto closestOnAabb(vec3 local, vec3 half) -> vec3 {
            return glm::clamp(local, -half, half);
        }

        auto closestOnObb(const Body::Quantum& body, vec3 half, vec3 world) -> vec3 {
            return fromBodyLocal(body, closestOnAabb(toBodyLocal(body, world), half));
        }

        struct Cohort {
            Compound::Id id;
            vec3 center;
            float radius;
            vector<Occupant> occupants;
        };

        auto worldOf(const Body::Quantum& body, vec3 local) -> vec3 {
            return vec3{body.position + dvec3{body.orientation * local}};
        }

        auto toLocal(const Body::Quantum& body, vec3 world) -> vec3 {
            return glm::conjugate(body.orientation) * vec3{dvec3{world} - body.position};
        }

        void ensureBvh(Crystal::Quantum& crystal) {
            if (not crystal.hull.bvh.nodes.empty())
                return;
            cookHullBvh(crystal.hull, crystal.shape);
        }

        auto nearestSurface(const Body::Quantum& shapeBody, const Crystal::Quantum& shapeCrystal, vec3 worldPoint, float radius, integer& faceIndex, vec3& closest, vec3& outward) -> float {
            const SurfaceHit hit = closestOnHull(shapeCrystal.hull, shapeCrystal.shape, toLocal(shapeBody, worldPoint));
            if (hit.face < 0)
                return 0.0f;
            if (hit.signedDistance >= radius)
                return 0.0f;
            faceIndex = hit.face;
            closest = worldOf(shapeBody, hit.localClosest);
            outward = shapeBody.orientation * hit.localOutward;
            const float length = glm::length(outward);
            if (length < minLength)
                return 0.0f;
            outward /= length;
            return radius - hit.signedDistance;
        }

        auto inverseMass(float mass) -> float {
            return mass > 0.0f ? 1.0f / mass : 0.0f;
        }

        void kickParticle(Particle& particle, vec3 delta) {
            particle.position += dvec3{delta};
        }

        void kickVertex(Crystal::Quantum& crystal, integer vertexIndex, vec3 delta) {
            if (vertexIndex < 0 or static_cast<std::size_t>(vertexIndex) >= crystal.particles.size())
                return;
            kickParticle(crystal.particles[static_cast<std::size_t>(vertexIndex)], delta);
        }

        auto triangleWeights(vec3 point, vec3 cornerA, vec3 cornerB, vec3 cornerC) -> vec3 {
            const vec3 n = glm::cross(cornerB - cornerA, cornerC - cornerA);
            const float area2 = glm::dot(n, n);
            if (area2 < minLength * minLength)
                return vec3{1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f};
            vec3 weight{glm::dot(glm::cross(cornerC - cornerB, point - cornerB), n), glm::dot(glm::cross(cornerA - cornerC, point - cornerC), n), glm::dot(glm::cross(cornerB - cornerA, point - cornerA), n)};
            if (weight.x < 0.0f)
                weight.x = 0.0f;
            if (weight.y < 0.0f)
                weight.y = 0.0f;
            if (weight.z < 0.0f)
                weight.z = 0.0f;
            const float sum = weight.x + weight.y + weight.z;
            if (sum < minLength)
                return vec3{1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f};
            return weight / sum;
        }

        void kickFaceSupports(Crystal::Quantum& crystal, const Body::Quantum& crystalBody, integer faceIndex, vec3 worldPoint, vec3 recoil, float solidMass) {
            if (solidMass <= 0.0f or faceIndex < 0 or static_cast<std::size_t>(faceIndex) >= crystal.hull.faces.size())
                return;
            const auto& face = crystal.hull.faces[static_cast<std::size_t>(faceIndex)];
            const std::size_t count = face.points.size();
            if (count < 2)
                return;
            float weights[8];
            integer ids[8];
            std::size_t used = 0;
            for (std::size_t index = 0; index < count and used < 8; ++index) {
                const integer id = face.points[index];
                if (id < 0 or static_cast<std::size_t>(id) >= crystal.particles.size() or static_cast<std::size_t>(id) >= crystal.shape.size())
                    continue;
                if (crystal.particles[static_cast<std::size_t>(id)].mass <= 0.0f)
                    continue;
                ids[used] = id;
                weights[used] = 1.0f;
                ++used;
            }
            if (used == 0)
                return;
            if (used == 3) {
                const vec3 bary = triangleWeights(worldPoint, worldOf(crystalBody, crystal.shape[static_cast<std::size_t>(ids[0])]), worldOf(crystalBody, crystal.shape[static_cast<std::size_t>(ids[1])]), worldOf(crystalBody, crystal.shape[static_cast<std::size_t>(ids[2])]));
                weights[0] = bary.x;
                weights[1] = bary.y;
                weights[2] = bary.z;
            }
            float sum = 0.0f;
            for (std::size_t index = 0; index < used; ++index)
                sum += weights[index];
            if (sum < minLength)
                return;
            for (std::size_t index = 0; index < used; ++index) {
                Particle& particle = crystal.particles[static_cast<std::size_t>(ids[index])];
                kickParticle(particle, recoil * (solidMass * (weights[index] / sum) / particle.mass));
            }
        }

        void scarFace(Crystal::Quantum& crystal, integer faceIndex, float impulseMag) {
            if (impulseMag <= 0.0f or Settings::cohesionWound <= 0.0f or faceIndex < 0 or static_cast<std::size_t>(faceIndex) >= crystal.hull.faces.size())
                return;
            const auto& face = crystal.hull.faces[static_cast<std::size_t>(faceIndex)];
            float clientMass = 0.0f;
            for (const integer id : face.points) {
                if (id < 0 or static_cast<std::size_t>(id) >= crystal.particles.size())
                    continue;
                const float mass = crystal.particles[static_cast<std::size_t>(id)].mass;
                if (mass > 0.0f)
                    clientMass += mass;
            }
            if (clientMass <= 0.0f)
                return;
            const float drop = Settings::cohesionWound * impulseMag / clientMass;
            for (const integer id : face.points) {
                if (id < 0 or static_cast<std::size_t>(id) >= crystal.particles.size())
                    continue;
                Particle& particle = crystal.particles[static_cast<std::size_t>(id)];
                if (particle.mass <= 0.0f)
                    continue;
                particle.cohesion -= drop;
            }
        }

        void addOccupant(vector<Occupant>& occupants, Body::Id id, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            auto* body = bodies.items.find(id);
            if (not body or body->radius <= 0.0f or body->totalMass <= 0.0f)
                return;
            if (auto* solid = solids.items.find(id)) {
                const auto type = solid->kind == Solid::Kind::box ? Endpoint::Type::box : Endpoint::Type::sphere;
                occupants.push_back(Occupant{type, id, vec3{body->position}, body->radius});
                return;
            }
            auto* crystal = crystals.items.find(id);
            if (not crystal)
                return;
            occupants.push_back(Occupant{Endpoint::Type::crystal, id, worldOf(*body, crystal->com), body->radius});
        }

        auto velocityOf(Body::Id id, Endpoint::Type type, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) -> vec3 {
            if (isSimple(type)) {
                auto* body = bodies.items.find(id);
                auto* solid = solids.items.find(id);
                if (not body or not solid)
                    return vec3{0.0f, 0.0f, 0.0f};
                return vec3{(body->position - solid->center.prev) / double(Particle::dt)};
            }
            auto* crystal = crystals.items.find(id);
            if (not crystal or crystal->particles.empty())
                return vec3{0.0f, 0.0f, 0.0f};
            dvec3 sum{0.0, 0.0, 0.0};
            for (const Particle& particle : crystal->particles)
                sum += particle.velocity();
            return vec3{sum / double(crystal->particles.size())};
        }

        auto omegaOf(const Body::Quantum& body, const Solid::Quantum& solid) -> vec3 {
            const float dt = Particle::dt;
            const quat qRel = glm::normalize(body.orientation * glm::conjugate(solid.prevOri));
            vec3 omega = (2.0f / dt) * vec3{qRel.x, qRel.y, qRel.z};
            if (qRel.w < 0.0f)
                omega = -omega;
            return omega;
        }

        auto velocityAt(const Body::Quantum& body, const Solid::Quantum& solid, vec3 worldPoint) -> vec3 {
            const vec3 linear = vec3{(body.position - solid.center.prev) / double(Particle::dt)};
            return linear + glm::cross(omegaOf(body, solid), worldPoint - vec3{body.position});
        }

        auto sphereInertia(const Body::Quantum& body) -> float {
            return 0.4f * body.totalMass * body.radius * body.radius;
        }

        auto spinWeight(const Body::Quantum& body, vec3 arm, vec3 tangent) -> float {
            const float inertia = sphereInertia(body);
            const vec3 rxt = glm::cross(arm, tangent);
            return inverseMass(body.totalMass) + (inertia > 1.0e-12f ? glm::dot(rxt, rxt) / inertia : 0.0f);
        }

        void kickSolid(Body::Quantum& body, Solid::Quantum& solid, vec3 arm, vec3 impulse) {
            if (body.totalMass <= 0.0f)
                return;
            body.position += dvec3{impulse / body.totalMass};
            solid.center.position = body.position;
            const float inertia = sphereInertia(body);
            if (inertia <= 1.0e-12f)
                return;
            const vec3 delta = glm::cross(arm, impulse) / inertia;
            const float angle = glm::length(delta);
            if (angle < minLength)
                return;
            body.orientation = glm::normalize(glm::angleAxis(angle, delta / angle) * body.orientation);
        }

        auto spheresOverlap(vec3 centerA, float radiusA, vec3 centerB, float radiusB) -> bool {
            const vec3 offset = centerB - centerA;
            const float limit = radiusA + radiusB;
            return glm::dot(offset, offset) <= limit * limit;
        }

        struct SimpleHit {
            bool hit;
            float penetration;
            vec3 fromFirstTowardSecond;
            vec3 point;
        };

        auto missSimple() -> SimpleHit {
            return SimpleHit{.hit = false, .penetration = 0.0f, .fromFirstTowardSecond = vec3{1.0f, 0.0f, 0.0f}, .point = vec3{0.0f, 0.0f, 0.0f}};
        }

        auto overlapSpheres(const Occupant& first, const Occupant& second) -> SimpleHit {
            const vec3 offset = second.center - first.center;
            const float distance = glm::length(offset);
            const float penetration = first.radius + second.radius - distance;
            if (penetration <= 0.0f)
                return missSimple();
            const vec3 fromFirstTowardSecond = distance >= minLength ? offset : vec3{1.0f, 0.0f, 0.0f};
            return SimpleHit{.hit = true, .penetration = penetration, .fromFirstTowardSecond = fromFirstTowardSecond, .point = first.center + glm::normalize(fromFirstTowardSecond) * first.radius};
        }

        auto overlapSphereObb(vec3 sphereCenter, float sphereRadius, const Body::Quantum& box, vec3 half) -> SimpleHit {
            const vec3 local = toBodyLocal(box, sphereCenter);
            const vec3 closestLocal = closestOnAabb(local, half);
            const vec3 delta = local - closestLocal;
            const float dist = glm::length(delta);
            vec3 localOutward;
            float penetration = 0.0f;
            if (dist < minLength) {
                const vec3 slack = half - glm::abs(local);
                int axis = 0;
                if (slack.y <= slack.x and slack.y <= slack.z)
                    axis = 1;
                else if (slack.z <= slack.x and slack.z <= slack.y)
                    axis = 2;
                localOutward = vec3{0.0f, 0.0f, 0.0f};
                localOutward[axis] = local[axis] >= 0.0f ? 1.0f : -1.0f;
                penetration = sphereRadius + slack[axis];
            } else {
                if (dist >= sphereRadius)
                    return missSimple();
                localOutward = delta / dist;
                penetration = sphereRadius - dist;
            }
            const vec3 outward = box.orientation * localOutward;
            return SimpleHit{.hit = true, .penetration = penetration, .fromFirstTowardSecond = -outward, .point = sphereCenter - outward * sphereRadius};
        }

        auto projectObb(const Body::Quantum& body, vec3 half, vec3 axis) -> float {
            const vec3 local = glm::abs(glm::conjugate(body.orientation) * axis);
            return glm::dot(local, half);
        }

        auto overlapBoxes(const Body::Quantum& first, vec3 halfA, const Body::Quantum& second, vec3 halfB) -> SimpleHit {
            const vec3 offset = vec3{second.position - first.position};
            const vec3 axisA[3] = {first.orientation * vec3{1.0f, 0.0f, 0.0f}, first.orientation * vec3{0.0f, 1.0f, 0.0f}, first.orientation * vec3{0.0f, 0.0f, 1.0f}};
            const vec3 axisB[3] = {second.orientation * vec3{1.0f, 0.0f, 0.0f}, second.orientation * vec3{0.0f, 1.0f, 0.0f}, second.orientation * vec3{0.0f, 0.0f, 1.0f}};
            vec3 bestAxis{1.0f, 0.0f, 0.0f};
            float bestOverlap = 1.0e30f;
            auto consider = [&](vec3 axis) -> bool {
                const float length = glm::length(axis);
                if (length < minLength)
                    return true;
                axis /= length;
                const float overlap = projectObb(first, halfA, axis) + projectObb(second, halfB, axis) - glm::abs(glm::dot(offset, axis));
                if (overlap <= 0.0f)
                    return false;
                if (overlap >= bestOverlap)
                    return true;
                bestOverlap = overlap;
                bestAxis = glm::dot(offset, axis) >= 0.0f ? axis : -axis;
                return true;
            };
            for (int index = 0; index < 3; ++index) {
                if (not consider(axisA[index]) or not consider(axisB[index]))
                    return missSimple();
            }
            for (int firstAxis = 0; firstAxis < 3; ++firstAxis) {
                for (int secondAxis = 0; secondAxis < 3; ++secondAxis) {
                    if (not consider(glm::cross(axisA[firstAxis], axisB[secondAxis])))
                        return missSimple();
                }
            }
            const vec3 onFirst = closestOnObb(first, halfA, vec3{second.position});
            const vec3 onSecond = closestOnObb(second, halfB, onFirst);
            return SimpleHit{.hit = true, .penetration = bestOverlap, .fromFirstTowardSecond = bestAxis, .point = 0.5f * (onFirst + onSecond)};
        }

        auto overlapSimples(const Occupant& first, const Occupant& second, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids) -> SimpleHit {
            auto* bodyA = bodies.items.find(first.body);
            auto* bodyB = bodies.items.find(second.body);
            auto* solidA = solids.items.find(first.body);
            auto* solidB = solids.items.find(second.body);
            if (not bodyA or not bodyB or not solidA or not solidB)
                return missSimple();
            if (first.type == Endpoint::Type::sphere and second.type == Endpoint::Type::sphere)
                return overlapSpheres(first, second);
            if (first.type == Endpoint::Type::sphere and second.type == Endpoint::Type::box) {
                auto hit = overlapSphereObb(first.center, first.radius, *bodyB, halfOf(*solidB, *bodyB));
                return hit;
            }
            if (first.type == Endpoint::Type::box and second.type == Endpoint::Type::sphere) {
                auto hit = overlapSphereObb(second.center, second.radius, *bodyA, halfOf(*solidA, *bodyA));
                if (not hit.hit)
                    return hit;
                hit.fromFirstTowardSecond = -hit.fromFirstTowardSecond;
                return hit;
            }
            return overlapBoxes(*bodyA, halfOf(*solidA, *bodyA), *bodyB, halfOf(*solidB, *bodyB));
        }

        auto nearestSurfaceObb(const Body::Quantum& shapeBody, const Crystal::Quantum& shapeCrystal, const Body::Quantum& box, vec3 half, integer& faceIndex, vec3& closest, vec3& outward) -> float {
            float best = 0.0f;
            integer bestFace = -1;
            vec3 bestClosest{0.0f, 0.0f, 0.0f};
            vec3 bestOutward{0.0f, 1.0f, 0.0f};
            auto consider = [&](vec3 world) {
                integer face = -1;
                vec3 hitClosest;
                vec3 hitOutward;
                const float depth = nearestSurface(shapeBody, shapeCrystal, world, 0.0f, face, hitClosest, hitOutward);
                if (depth <= best)
                    return;
                best = depth;
                bestFace = face;
                bestClosest = hitClosest;
                bestOutward = hitOutward;
            };
            consider(vec3{box.position});
            for (int sx = -1; sx <= 1; sx += 2)
                for (int sy = -1; sy <= 1; sy += 2)
                    for (int sz = -1; sz <= 1; sz += 2)
                        consider(fromBodyLocal(box, half * vec3{static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(sz)}));
            const SurfaceHit hullHit = closestOnHull(shapeCrystal.hull, shapeCrystal.shape, toLocal(shapeBody, vec3{box.position}));
            if (hullHit.face >= 0)
                consider(closestOnObb(box, half, worldOf(shapeBody, hullHit.localClosest)));
            if (best <= 0.0f)
                return 0.0f;
            faceIndex = bestFace;
            closest = bestClosest;
            outward = bestOutward;
            return best;
        }

        auto overlapPointObb(vec3 point, const Body::Quantum& box, vec3 half) -> SimpleHit {
            return overlapSphereObb(point, 0.0f, box, half);
        }

        auto firstOnAabbSegment(vec3 p0, vec3 p1, vec3 half) -> float {
            if (glm::abs(p0.x) <= half.x and glm::abs(p0.y) <= half.y and glm::abs(p0.z) <= half.z)
                return 0.0f;
            const vec3 delta = p1 - p0;
            float tMin = 0.0f;
            float tMax = 1.0f;
            for (int axis = 0; axis < 3; ++axis) {
                const float origin = p0[axis];
                const float span = delta[axis];
                const float limit = half[axis];
                if (glm::abs(span) < minLength) {
                    if (origin < -limit or origin > limit)
                        return -1.0f;
                    continue;
                }
                float t0 = (-limit - origin) / span;
                float t1 = (limit - origin) / span;
                if (t0 > t1) {
                    const float swap = t0;
                    t0 = t1;
                    t1 = swap;
                }
                tMin = glm::max(tMin, t0);
                tMax = glm::min(tMax, t1);
                if (tMin > tMax)
                    return -1.0f;
            }
            if (tMin > 1.0f)
                return -1.0f;
            return tMin;
        }

        void pushContact(State& state, integer candidate, Endpoint first, Endpoint second, vec3 point, vec3 fromFirstTowardSecond, float penetration, vec3 velocityFirst, vec3 velocitySecond) {
            const float length = glm::length(fromFirstTowardSecond);
            if (length < minLength or penetration <= 0.0f)
                return;
            const vec3 normal = fromFirstTowardSecond / length;
            state.contacts.push_back(Contact{
                .a = first,
                .b = second,
                .point = point,
                .normal = normal,
                .penetration = penetration,
                .candidate = candidate,
                .correction = 0.0f,
                .relativeNormalSpeed = glm::dot(velocityFirst - velocitySecond, normal),
            });
        }

        void contactSolidSolid(State& state, integer candidate, const Occupant& first, const Occupant& second, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            const SimpleHit hit = overlapSimples(first, second, bodies, solids);
            if (not hit.hit)
                return;
            pushContact(state, candidate, Endpoint{first.type, first.body, 0}, Endpoint{second.type, second.body, 0}, hit.point, hit.fromFirstTowardSecond, hit.penetration, velocityOf(first.body, first.type, bodies, solids, crystals), velocityOf(second.body, second.type, bodies, solids, crystals));
        }

        void contactParticlesVsShape(State& state, integer candidate, const Occupant& particleSide, const Occupant& shapeSide, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            auto* particleCrystal = crystals.items.find(particleSide.body);
            auto* shapeBody = bodies.items.find(shapeSide.body);
            auto* shapeCrystal = crystals.items.find(shapeSide.body);
            if (not particleCrystal or not shapeBody or not shapeCrystal)
                return;
            if (particleCrystal->particles.empty() or particleCrystal->particles.size() != particleCrystal->shape.size())
                return;
            const vec3 shapeVelocity = velocityOf(shapeSide.body, shapeSide.type, bodies, solids, crystals);
            for (std::size_t vertexIndex = 0; vertexIndex < particleCrystal->particles.size(); ++vertexIndex) {
                integer faceIndex = -1;
                vec3 closest;
                vec3 outward;
                const vec3 vertex = vec3{particleCrystal->particles[vertexIndex].position};
                const float depth = nearestSurface(*shapeBody, *shapeCrystal, vertex, 0.0f, faceIndex, closest, outward);
                if (depth <= 0.0f)
                    continue;
                const vec3 vertexVelocity = vec3{particleCrystal->particles[vertexIndex].velocity()};
                pushContact(state, candidate, Endpoint{Endpoint::Type::crystal, particleSide.body, static_cast<integer>(vertexIndex)}, Endpoint{Endpoint::Type::crystal, shapeSide.body, faceIndex}, closest, -outward, depth, vertexVelocity, shapeVelocity);
            }
        }

        void contactSolidVsShape(State& state, integer candidate, const Occupant& solid, const Occupant& shapeSide, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            auto* shapeBody = bodies.items.find(shapeSide.body);
            auto* shapeCrystal = crystals.items.find(shapeSide.body);
            if (not shapeBody or not shapeCrystal)
                return;
            integer faceIndex = -1;
            vec3 closest;
            vec3 outward;
            float depth = 0.0f;
            if (solid.type == Endpoint::Type::box) {
                auto* boxBody = bodies.items.find(solid.body);
                auto* boxSolid = solids.items.find(solid.body);
                if (not boxBody or not boxSolid)
                    return;
                depth = nearestSurfaceObb(*shapeBody, *shapeCrystal, *boxBody, halfOf(*boxSolid, *boxBody), faceIndex, closest, outward);
            } else {
                depth = nearestSurface(*shapeBody, *shapeCrystal, solid.center, solid.radius, faceIndex, closest, outward);
            }
            if (depth <= 0.0f)
                return;
            pushContact(state, candidate, Endpoint{solid.type, solid.body, 0}, Endpoint{Endpoint::Type::crystal, shapeSide.body, faceIndex}, closest, -outward, depth, velocityOf(solid.body, solid.type, bodies, solids, crystals), velocityOf(shapeSide.body, shapeSide.type, bodies, solids, crystals));
        }

        void contactParticlesVsSolid(State& state, integer candidate, const Occupant& particleSide, const Occupant& solid, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            auto* particleCrystal = crystals.items.find(particleSide.body);
            if (not particleCrystal or particleCrystal->particles.empty())
                return;
            const vec3 solidVelocity = velocityOf(solid.body, solid.type, bodies, solids, crystals);
            if (solid.type == Endpoint::Type::box) {
                auto* boxBody = bodies.items.find(solid.body);
                auto* boxSolid = solids.items.find(solid.body);
                if (not boxBody or not boxSolid)
                    return;
                const vec3 half = halfOf(*boxSolid, *boxBody);
                for (std::size_t vertexIndex = 0; vertexIndex < particleCrystal->particles.size(); ++vertexIndex) {
                    const vec3 vertex = vec3{particleCrystal->particles[vertexIndex].position};
                    const SimpleHit hit = overlapPointObb(vertex, *boxBody, half);
                    if (not hit.hit)
                        continue;
                    const vec3 vertexVelocity = vec3{particleCrystal->particles[vertexIndex].velocity()};
                    pushContact(state, candidate, Endpoint{Endpoint::Type::crystal, particleSide.body, static_cast<integer>(vertexIndex)}, Endpoint{solid.type, solid.body, 0}, hit.point, hit.fromFirstTowardSecond, hit.penetration, vertexVelocity, solidVelocity);
                }
                return;
            }
            const vec3 center = solid.center;
            for (std::size_t vertexIndex = 0; vertexIndex < particleCrystal->particles.size(); ++vertexIndex) {
                const vec3 vertex = vec3{particleCrystal->particles[vertexIndex].position};
                const vec3 offset = vertex - center;
                const float distance = glm::length(offset);
                const float penetration = solid.radius - distance;
                if (penetration <= 0.0f)
                    continue;
                const vec3 away = distance >= minLength ? offset / distance : vec3{1.0f, 0.0f, 0.0f};
                const vec3 vertexVelocity = vec3{particleCrystal->particles[vertexIndex].velocity()};
                pushContact(state, candidate, Endpoint{Endpoint::Type::crystal, particleSide.body, static_cast<integer>(vertexIndex)}, Endpoint{solid.type, solid.body, 0}, center + away * solid.radius, -away, penetration, vertexVelocity, solidVelocity);
            }
        }

        void clearSolidSpin(const Body::Quantum& body, Solid::Quantum& solid) {
            solid.prevOri = body.orientation;
        }

        void frictionSolidCrystal(Contact& contact, float normalStep, float live, Body::Quantum& body, Solid::Quantum& solid, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            clearSolidSpin(body, solid);
            if (live <= 0.0f)
                return;
            const vec3 arm = contact.point - vec3{body.position};
            const vec3 slideVec = velocityAt(body, solid, contact.point) - velocityOf(contact.b.body, contact.b.type, bodies, solids, crystals);
            const vec3 tangentVel = slideVec - contact.normal * glm::dot(slideVec, contact.normal);
            const float slide = glm::length(tangentVel);
            if (slide < minLength)
                return;
            const vec3 tangent = tangentVel / slide;
            const float weight = spinWeight(body, arm, tangent);
            if (weight <= 0.0f)
                return;
            float impulse = slide * Particle::dt / weight;
            const float limit = solidFriction * live * body.totalMass * normalStep;
            if (impulse > limit)
                impulse = limit;
            kickSolid(body, solid, arm, -tangent * impulse);
        }

        void frictionSolidSolid(Contact& contact, float normalStep, Body::Quantum& bodyA, Solid::Quantum& solidA, Body::Quantum& bodyB, Solid::Quantum& solidB) {
            clearSolidSpin(bodyA, solidA);
            clearSolidSpin(bodyB, solidB);
            const vec3 armA = contact.point - vec3{bodyA.position};
            const vec3 armB = contact.point - vec3{bodyB.position};
            const vec3 slideVec = velocityAt(bodyA, solidA, contact.point) - velocityAt(bodyB, solidB, contact.point);
            const vec3 tangentVel = slideVec - contact.normal * glm::dot(slideVec, contact.normal);
            const float slide = glm::length(tangentVel);
            if (slide < minLength)
                return;
            const vec3 tangent = tangentVel / slide;
            const float weight = spinWeight(bodyA, armA, tangent) + spinWeight(bodyB, armB, tangent);
            if (weight <= 0.0f)
                return;
            const float invSum = inverseMass(bodyA.totalMass) + inverseMass(bodyB.totalMass);
            if (invSum <= 0.0f)
                return;
            float impulse = slide * Particle::dt / weight;
            const float limit = solidFriction * (normalStep / invSum);
            if (impulse > limit)
                impulse = limit;
            kickSolid(bodyA, solidA, armA, -tangent * impulse);
            kickSolid(bodyB, solidB, armB, tangent * impulse);
        }

        // Reflect Verlet normal step via center.prev only — position already on the surface. Crystal particles do not use this.
        // `live` soft-scales restitution (0 → e=0 stick; 1 → full solidRestitution). Separation is unchanged.
        void bounceSolid(Body::Quantum& body, Solid::Quantum& solid, vec3 normal, float otherNormalStep, float live) {
            const float vn = float(glm::dot(body.position - solid.center.prev, dvec3{normal})) - otherNormalStep;
            if (vn <= 0.0f)
                return;
            solid.center.prev += dvec3{normal * ((1.0f + solidRestitution * live) * vn)};
        }

        void bounceSolidSolid(Body::Quantum& bodyA, Solid::Quantum& solidA, Body::Quantum& bodyB, Solid::Quantum& solidB, vec3 normal) {
            const float weightA = inverseMass(bodyA.totalMass);
            const float weightB = inverseMass(bodyB.totalMass);
            const float weightSum = weightA + weightB;
            if (weightSum <= 0.0f)
                return;
            const float vn = float(glm::dot((bodyA.position - solidA.center.prev) - (bodyB.position - solidB.center.prev), dvec3{normal}));
            if (vn <= 0.0f)
                return;
            const float jump = (1.0f + solidRestitution) * vn / weightSum;
            solidA.center.prev += dvec3{normal * (jump * weightA)};
            solidB.center.prev -= dvec3{normal * (jump * weightB)};
        }

        auto solidCrystalLive(float closingSpeed) -> float {
            if (Settings::solidLiveSpeed <= 0.0f)
                return 1.0f;
            const float t = closingSpeed / Settings::solidLiveSpeed;
            if (t <= 0.0f)
                return 0.0f;
            if (t >= 1.0f)
                return 1.0f;
            return t * t * (3.0f - 2.0f * t);
        }

        void respondSolidCrystal(Contact& contact, float remaining, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            auto* body = bodies.items.find(contact.a.body);
            auto* solid = solids.items.find(contact.a.body);
            if (not body or not solid)
                return;
            body->position -= dvec3{contact.normal * remaining};
            solid->center.position = body->position;
            const float live = solidCrystalLive(glm::max(0.0f, contact.relativeNormalSpeed));
            const vec3 otherVelocity = velocityOf(contact.b.body, contact.b.type, bodies, solids, crystals);
            bounceSolid(*body, *solid, contact.normal, glm::dot(otherVelocity, contact.normal) * Particle::dt, live);
            frictionSolidCrystal(contact, remaining, live, *body, *solid, bodies, solids, crystals);
            auto* crystal = crystals.items.find(contact.b.body);
            auto* crystalBody = bodies.items.find(contact.b.body);
            if (not crystal or not crystalBody)
                return;
            kickFaceSupports(*crystal, *crystalBody, contact.b.face, contact.point, contact.normal * remaining, body->totalMass);
        }

        void respondSolidSolid(Contact& contact, float remaining, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids) {
            auto* bodyA = bodies.items.find(contact.a.body);
            auto* bodyB = bodies.items.find(contact.b.body);
            auto* solidA = solids.items.find(contact.a.body);
            auto* solidB = solids.items.find(contact.b.body);
            if (not bodyA or not bodyB or not solidA or not solidB)
                return;
            const float weightA = inverseMass(bodyA->totalMass);
            const float weightB = inverseMass(bodyB->totalMass);
            const float weightSum = weightA + weightB;
            if (weightSum <= 0.0f)
                return;
            bodyA->position -= dvec3{contact.normal * (remaining * (weightA / weightSum))};
            bodyB->position += dvec3{contact.normal * (remaining * (weightB / weightSum))};
            solidA->center.position = bodyA->position;
            solidB->center.position = bodyB->position;
            bounceSolidSolid(*bodyA, *solidA, *bodyB, *solidB, contact.normal);
            frictionSolidSolid(contact, remaining, *bodyA, *solidA, *bodyB, *solidB);
        }

        void kickContactVertex(Contact& contact, float remaining, fqsm::Direct<Crystal> crystals) {
            auto* particleCrystal = crystals.items.find(contact.a.body);
            if (not particleCrystal)
                return;
            kickVertex(*particleCrystal, contact.a.face, -contact.normal * remaining);
        }

        void solveContact(Contact& contact, float remaining, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            if (isSimple(contact.a.type) and isSimple(contact.b.type))
                respondSolidSolid(contact, remaining, bodies, solids);
            else if (isSimple(contact.a.type))
                respondSolidCrystal(contact, remaining, bodies, solids, crystals);
            else if (not isSimple(contact.b.type))
                kickContactVertex(contact, remaining, crystals);
        }

        auto collectOccupants(Compound::Id host, const Compound::Quantum& compound, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) -> vector<Occupant> {
            vector<Occupant> occupants;
            addOccupant(occupants, host, bodies, solids, crystals);
            for (const Body::Id member : compound.members) {
                if (member == host)
                    continue;
                addOccupant(occupants, member, bodies, solids, crystals);
            }
            return occupants;
        }

        auto boundOf(const vector<Occupant>& occupants, vec3& center, float& radius) -> bool {
            if (occupants.empty())
                return false;
            center = vec3{0.0f, 0.0f, 0.0f};
            for (const Occupant& occupant : occupants)
                center += occupant.center;
            center /= static_cast<float>(occupants.size());
            radius = 0.0f;
            for (const Occupant& occupant : occupants)
                radius = glm::max(radius, glm::length(occupant.center - center) + occupant.radius);
            return true;
        }

        void pairOccupants(State& state, const Occupant& first, const Occupant& second, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            if (first.body == second.body)
                return;
            if (not spheresOverlap(first.center, first.radius, second.center, second.radius))
                return;
            const integer candidate = static_cast<integer>(state.candidates.size());
            state.candidates.push_back(Candidate{
                .a = Endpoint{first.type, first.body, 0},
                .b = Endpoint{second.type, second.body, 0},
            });
            if (isSimple(first.type) and isSimple(second.type))
                contactSolidSolid(state, candidate, first, second, bodies, solids, crystals);
            else if (isSimple(first.type) and second.type == Endpoint::Type::crystal) {
                contactSolidVsShape(state, candidate, first, second, bodies, solids, crystals);
                contactParticlesVsSolid(state, candidate, second, first, bodies, solids, crystals);
            } else if (first.type == Endpoint::Type::crystal and isSimple(second.type)) {
                contactSolidVsShape(state, candidate, second, first, bodies, solids, crystals);
                contactParticlesVsSolid(state, candidate, first, second, bodies, solids, crystals);
            } else {
                contactParticlesVsShape(state, candidate, first, second, bodies, solids, crystals);
                contactParticlesVsShape(state, candidate, second, first, bodies, solids, crystals);
            }
        }

        auto firstOnSphere(vec3 p0, vec3 p1, vec3 center, float radius) -> float {
            const vec3 d = p1 - p0;
            const vec3 f = p0 - center;
            const float r2 = radius * radius;
            const float c = glm::dot(f, f) - r2;
            if (c <= 0.0f)
                return 0.0f;
            const float a = glm::dot(d, d);
            if (a < minLength)
                return -1.0f;
            const float b = 2.0f * glm::dot(f, d);
            const float disc = b * b - 4.0f * a * c;
            if (disc < 0.0f)
                return -1.0f;
            const float t = (-b - std::sqrt(disc)) / (2.0f * a);
            if (t < 0.0f or t > 1.0f)
                return -1.0f;
            return t;
        }

        auto lookAlong(vec3 forward, quat fallback) -> quat {
            const float length = glm::length(forward);
            if (length < minLength)
                return fallback;
            const vec3 dir = forward / length;
            vec3 up{0.0f, 1.0f, 0.0f};
            if (glm::abs(glm::dot(dir, up)) > 0.99f)
                up = vec3{1.0f, 0.0f, 0.0f};
            return glm::quatLookAt(dir, up);
        }

        auto faceInvMassAndVelocity(const Crystal::Quantum& crystal, integer faceIndex, vec3& velocity) -> float {
            velocity = vec3{0.0f, 0.0f, 0.0f};
            if (faceIndex < 0 or static_cast<std::size_t>(faceIndex) >= crystal.hull.faces.size())
                return 0.0f;
            const auto& face = crystal.hull.faces[static_cast<std::size_t>(faceIndex)];
            double mass = 0.0;
            dvec3 momentum{0.0, 0.0, 0.0};
            for (const integer id : face.points) {
                if (id < 0 or static_cast<std::size_t>(id) >= crystal.particles.size())
                    continue;
                const Particle& particle = crystal.particles[static_cast<std::size_t>(id)];
                if (particle.mass <= 0.0f)
                    continue;
                mass += double(particle.mass);
                momentum += particle.velocity() * double(particle.mass);
            }
            if (mass <= 1.0e-12)
                return 0.0f;
            velocity = vec3{momentum / mass};
            return float(1.0 / mass);
        }

        auto pierceBlend(float closingSpeed) -> float {
            if (closingSpeed <= rayPierceBegin)
                return 0.0f;
            if (closingSpeed >= rayPierceFull)
                return 1.0f;
            const float t = (closingSpeed - rayPierceBegin) / (rayPierceFull - rayPierceBegin);
            return t * t * (3.0f - 2.0f * t);
        }

    }

    void State::build(Stewarding context) {
        candidates.clear();
        contacts.clear();
        auto bodies = context.direct<Body>();
        auto solids = context.direct<Solid>();
        auto crystals = context.direct<Crystal>();
        auto compounds = context.direct<Compound>();
        for (auto [id, crystal] : crystals.items)
            ensureBvh(crystal);
        vector<Cohort> cohorts;
        for (auto [id, compound] : compounds.items) {
            auto occupants = collectOccupants(id, compound, bodies, solids, crystals);
            vec3 center;
            float radius;
            if (not boundOf(occupants, center, radius))
                continue;
            cohorts.push_back(Cohort{id, center, radius, std::move(occupants)});
        }
        for (std::size_t first = 0; first < cohorts.size(); ++first) {
            for (std::size_t second = first + 1; second < cohorts.size(); ++second) {
                if (not spheresOverlap(cohorts[first].center, cohorts[first].radius, cohorts[second].center, cohorts[second].radius))
                    continue;
                for (const Occupant& occupantA : cohorts[first].occupants) {
                    for (const Occupant& occupantB : cohorts[second].occupants)
                        pairOccupants(*this, occupantA, occupantB, bodies, solids, crystals);
                }
            }
        }
    }

    void State::solve(Stewarding context) {
        static bool wasHit = false;
        static int quiet = 0;
        if (contacts.empty()) {
            if (wasHit) {
                wasHit = false;
                quiet = 0;
                base::message("phys::hit end");
            }
            return;
        }
        auto bodies = context.direct<Body>();
        auto solids = context.direct<Solid>();
        auto crystals = context.direct<Crystal>();
        float maxPen = 0.0f;
        float vn = 0.0f;
        vec3 nrm{0.0f, 0.0f, 0.0f};
        for (const Contact& contact : contacts) {
            if (contact.penetration <= maxPen)
                continue;
            maxPen = contact.penetration;
            vn = contact.relativeNormalSpeed;
            nrm = contact.normal;
        }
        auto* bodyA = bodies.items.find(contacts.front().a.body);
        auto* bodyB = bodies.items.find(contacts.front().b.body);
        const dvec3 originA = bodyA ? bodyA->position : dvec3{0.0, 0.0, 0.0};
        const dvec3 originB = bodyB ? bodyB->position : dvec3{0.0, 0.0, 0.0};
        for (Contact& contact : contacts) {
            if (contact.penetration <= 0.0f)
                continue;
            solveContact(contact, contact.penetration, bodies, solids, crystals);
            contact.correction = contact.penetration;
        }
        float corr = 0.0f;
        int live = 0;
        for (const Contact& contact : contacts) {
            corr += contact.correction;
            if (contact.correction > 0.0f)
                ++live;
        }
        const float dA = bodyA ? float(glm::length(bodyA->position - originA)) : 0.0f;
        const float dB = bodyB ? float(glm::length(bodyB->position - originB)) : 0.0f;
        const bool shout = not wasHit or quiet == 0;
        wasHit = true;
        quiet = (quiet + 1) % 10;
        if (shout)
            base::message("phys::hit n={} live={} pen={:.2f} vn={:.1f} corr={:.2f} dA={:.2f} dB={:.2f} nrm={:.2f},{:.2f},{:.2f}", contacts.size(), live, maxPen, vn, corr, dA, dB, nrm.x, nrm.y, nrm.z);
    }

    void State::traceRays(Stewarding context) {
        auto bodies = context.direct<Body>();
        auto solids = context.direct<Solid>();
        auto crystals = context.direct<Crystal>();
        auto rays = context.direct<Ray>();
        for (auto [_, crystal] : crystals.items)
            ensureBvh(crystal);
        for (auto [id, ray] : rays.items) {
            auto* body = bodies.items.find(id);
            if (not body or ray.core.mass <= 0.0f)
                continue;
            const vec3 start = vec3{ray.core.prev};
            const vec3 end = vec3{ray.core.position};
            const float rayRadius = glm::max(body->radius, 0.0f);
            const vec3 span = end - start;
            float bestT = 2.0f;
            bool hitSolid = false;
            Body::Id other = id;
            integer face = -1;
            vec3 point = end;
            vec3 normal{0.0f, 1.0f, 0.0f};
            for (auto [solidId, solid] : solids.items) {
                auto* solidBody = bodies.items.find(solidId);
                if (not solidBody or solidBody->radius <= 0.0f)
                    continue;
                float t = -1.0f;
                vec3 away{0.0f, 1.0f, 0.0f};
                if (solid.kind == Solid::Kind::box) {
                    const vec3 half = halfOf(solid, *solidBody) + vec3{rayRadius, rayRadius, rayRadius};
                    t = firstOnAabbSegment(toBodyLocal(*solidBody, start), toBodyLocal(*solidBody, end), half);
                    if (t < 0.0f or t >= bestT)
                        continue;
                    const vec3 at = start + t * span;
                    away = at - closestOnObb(*solidBody, halfOf(solid, *solidBody), at);
                } else {
                    t = firstOnSphere(start, end, vec3{solidBody->position}, solidBody->radius + rayRadius);
                    if (t < 0.0f or t >= bestT)
                        continue;
                    away = start + t * span - vec3{solidBody->position};
                }
                const float awayLen = glm::length(away);
                if (awayLen < minLength)
                    continue;
                bestT = t;
                hitSolid = true;
                other = solidId;
                face = -1;
                point = start + t * span;
                normal = away / awayLen;
            }
            for (auto [crystalId, crystal] : crystals.items) {
                auto* crystalBody = bodies.items.find(crystalId);
                if (not crystalBody)
                    continue;
                const SegmentHit hit = firstOnHull(crystal.hull, crystal.shape, toLocal(*crystalBody, start), toLocal(*crystalBody, end), rayRadius);
                if (hit.face < 0 or hit.t >= bestT)
                    continue;
                vec3 away = crystalBody->orientation * hit.localOutward;
                const float awayLen = glm::length(away);
                if (awayLen < minLength)
                    continue;
                bestT = hit.t;
                hitSolid = false;
                other = crystalId;
                face = hit.face;
                point = worldOf(*crystalBody, hit.localClosest);
                normal = away / awayLen;
            }
            if (bestT > 1.0f)
                continue;
            const vec3 vRay = vec3{ray.core.velocity()};
            vec3 vOther{0.0f, 0.0f, 0.0f};
            float invOther = 0.0f;
            vec3 arm{0.0f, 0.0f, 0.0f};
            if (hitSolid) {
                auto* solidBody = bodies.items.find(other);
                auto* solid = solids.items.find(other);
                if (not solidBody or not solid)
                    continue;
                vOther = velocityAt(*solidBody, *solid, point);
                arm = point - vec3{solidBody->position};
                invOther = spinWeight(*solidBody, arm, normal);
            } else {
                auto* crystal = crystals.items.find(other);
                if (not crystal)
                    continue;
                invOther = faceInvMassAndVelocity(*crystal, face, vOther);
            }
            const float vn = glm::dot(vRay - vOther, normal);
            if (vn >= 0.0f)
                continue;
            const float invRay = inverseMass(ray.core.mass);
            const float denom = invRay + invOther;
            if (denom <= 0.0f)
                continue;
            const float pierce = pierceBlend(-vn);
            const float restitution = rayRestitution * (1.0f - rayPierceKeep * pierce);
            const float j = -(1.0f + restitution) * vn / denom;
            if (hitSolid) {
                auto* solidBody = bodies.items.find(other);
                auto* solid = solids.items.find(other);
                if (solidBody and solid)
                    kickSolid(*solidBody, *solid, arm, -normal * (j * Particle::dt));
            } else {
                auto* crystal = crystals.items.find(other);
                auto* crystalBody = bodies.items.find(other);
                if (crystal and crystalBody) {
                    kickFaceSupports(*crystal, *crystalBody, face, point, -normal * (j * Particle::dt / ray.core.mass), ray.core.mass);
                    scarFace(*crystal, face, glm::length(ray.core.impulse()));
                }
            }
            const vec3 vNew = vRay + normal * (j * invRay);
            if (glm::dot(vNew, normal) >= 0.0f) {
                const dvec3 hitPos = dvec3{point + normal * raySkin};
                ray.core.position = hitPos;
                ray.core.prev = hitPos - dvec3{vNew * Particle::dt};
            } else {
                ray.core.prev = ray.core.position - dvec3{vNew * Particle::dt};
            }
            body->position = ray.core.position;
            body->orientation = lookAlong(vNew, body->orientation);
        }
    }

}
