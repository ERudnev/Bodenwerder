#include "physics/resting.h"
#include "physics/settings.h"

#include <eltanin/physics/resting.q1.h>
#include <eltanin/physics/rigid.q1.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace eltanin::phys::collision {

    using rigid::Solid;
    using rigid::Crystal;

    namespace {

        constexpr float minLength = 1.0e-8f;

        auto rotationDistance(quat first, quat second) -> float {
            const quat delta = glm::normalize(first * glm::conjugate(second));
            return 2.0f * std::acos(glm::clamp(glm::abs(delta.w), 0.0f, 1.0f));
        }

        auto inverseMass(float mass) -> float {
            return mass > 0.0f ? 1.0f / mass : 0.0f;
        }

        auto sphereInertia(const Body::Quantum& body) -> float {
            return 0.4f * body.totalMass * body.radius * body.radius;
        }

        auto omegaOfSolid(const Body::Quantum& body, const Solid::Quantum& solid) -> vec3 {
            const float dt = float(Settings::fixedStep);
            const quat qRel = glm::normalize(body.orientation * glm::conjugate(solid.prevOri));
            vec3 omega = (2.0f / dt) * vec3{qRel.x, qRel.y, qRel.z};
            if (qRel.w < 0.0f)
                omega = -omega;
            return omega;
        }

        auto shapeSignature(Body::Id id, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) -> integer {
            if (auto* crystal = crystals.items.find(id))
                return static_cast<integer>(crystal->particles.size());
            if (auto* solid = solids.items.find(id))
                return solid->kind == Solid::Kind::box ? -2 : -1;
            return 0;
        }

        auto worldOf(const dvec3& origin, quat orientation, vec3 local) -> vec3 {
            return vec3{origin} + orientation * local;
        }

        struct Slot {
            Body::Id id;
            dvec3 origin;
            quat orientation;
            dvec3 com;
            vec3 localCom;
            vec3 linear;
            vec3 omega;
            float invMass;
            float invInertia;
            dvec3 origin0;
            quat orientation0;
            vec3 linear0;
            vec3 omega0;
        };

        auto readSlot(Body::Id id, const Body::Quantum& body, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) -> Slot {
            const float inertia = sphereInertia(body);
            Slot slot{
                .id = id,
                .origin = body.position,
                .orientation = body.orientation,
                .com = body.position,
                .localCom = vec3{0.0f, 0.0f, 0.0f},
                .linear = vec3{0.0f, 0.0f, 0.0f},
                .omega = vec3{0.0f, 0.0f, 0.0f},
                .invMass = inverseMass(body.totalMass),
                .invInertia = inertia > minLength ? 1.0f / inertia : 0.0f,
                .origin0 = body.position,
                .orientation0 = body.orientation,
                .linear0 = vec3{0.0f, 0.0f, 0.0f},
                .omega0 = vec3{0.0f, 0.0f, 0.0f},
            };
            if (auto* solid = solids.items.find(id)) {
                slot.linear = vec3{(body.position - solid->center.prev) / Settings::fixedStep};
                slot.omega = omegaOfSolid(body, *solid);
                slot.linear0 = slot.linear;
                slot.omega0 = slot.omega;
                return slot;
            }
            auto* crystal = crystals.items.find(id);
            if (not crystal or crystal->particles.empty() or body.totalMass <= 0.0f)
                return slot;
            slot.localCom = crystal->com;
            slot.com = body.position + dvec3{body.orientation * crystal->com};
            dvec3 linear{0.0, 0.0, 0.0};
            dvec3 angular{0.0, 0.0, 0.0};
            const double dt = Settings::fixedStep;
            for (const Particle& particle : crystal->particles) {
                if (particle.mass <= 0.0f)
                    continue;
                const dvec3 velocity = (particle.position - particle.prev) / dt;
                linear += velocity * double(particle.mass);
                angular += glm::cross(particle.position - slot.com, velocity * double(particle.mass));
            }
            slot.linear = vec3{linear / double(body.totalMass)};
            slot.omega = inertia > minLength ? vec3{angular / double(inertia)} : vec3{0.0f, 0.0f, 0.0f};
            slot.linear0 = slot.linear;
            slot.omega0 = slot.omega;
            return slot;
        }

        void refreshCom(Slot& slot) {
            slot.com = slot.origin + dvec3{slot.orientation * slot.localCom};
        }

        struct RestError {
            vec3 normal;
            vec3 tangent;
            float stretch;
            float twist;
            float reducedMass;
            float compressionImpulse;
        };

        auto restError(const Resting::Quantum& rest, const dvec3& originA, quat oriA, const dvec3& originB, quat oriB, float invMassA, float invMassB) -> RestError {
            vec3 normal = oriA * rest.normalFirst;
            const float normalLength = glm::length(normal);
            normal = normalLength > minLength ? normal / normalLength : vec3{0.0f, 1.0f, 0.0f};
            const vec3 delta = worldOf(originB, oriB, rest.anchorSecond) - worldOf(originA, oriA, rest.anchorFirst);
            const float stretch = glm::dot(delta, normal);
            const float invSum = invMassA + invMassB;
            const float reduced = invSum > 0.0f ? 1.0f / invSum : 0.0f;
            return RestError{normal, delta - normal * stretch, stretch, rotationDistance(oriB, glm::normalize(oriA * rest.relativeOrientation)), reduced, reduced * glm::max(-stretch, 0.0f) / float(Settings::fixedStep)};
        }

        auto breaksRest(const Resting::Quantum& rest, const RestError& error) -> bool {
            if (error.stretch > Settings::Resting::tensileMeters or error.twist > Settings::Resting::twistRadians or error.reducedMass <= 0.0f)
                return true;
            const float tangentImpulse = glm::length(error.tangent) * error.reducedMass / float(Settings::fixedStep);
            const float supported = Settings::Resting::staticFriction * (glm::max(rest.normalLoad, error.compressionImpulse) + error.reducedMass * Settings::Resting::adhesiveSpeed);
            return tangentImpulse > supported;
        }

        auto sameGeometry(const Resting::Quantum& rest, const Body::Quantum& first, const Body::Quantum& second, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) -> bool {
            const float scaleFirst = glm::max(first.radius, 1.0f);
            const float scaleSecond = glm::max(second.radius, 1.0f);
            return shapeSignature(rest.first, solids, crystals) == rest.firstShape and shapeSignature(rest.second, solids, crystals) == rest.secondShape and glm::abs(first.radius - rest.firstRadius) <= scaleFirst * 1.0e-4f and glm::abs(second.radius - rest.secondRadius) <= scaleSecond * 1.0e-4f;
        }

        void correctPose(Slot& first, Slot& second, const Resting::Quantum& rest) {
            const float invSum = first.invMass + second.invMass;
            if (invSum <= 0.0f)
                return;
            const dvec3 error = second.origin - (first.origin + dvec3{first.orientation * rest.relativeOffset});
            first.origin += error * double(first.invMass / invSum);
            second.origin -= error * double(second.invMass / invSum);
            quat delta = glm::normalize(second.orientation * glm::conjugate(glm::normalize(first.orientation * rest.relativeOrientation)));
            if (delta.w < 0.0f)
                delta = -delta;
            const float angle = 2.0f * std::acos(glm::clamp(delta.w, 0.0f, 1.0f));
            const float sine = glm::length(vec3{delta.x, delta.y, delta.z});
            if (angle <= minLength or sine <= minLength)
                return;
            const vec3 axis = vec3{delta.x, delta.y, delta.z} / sine;
            const float angularSum = first.invInertia + second.invInertia;
            if (angularSum <= 0.0f)
                return;
            first.orientation = glm::normalize(glm::angleAxis(angle * first.invInertia / angularSum, axis) * first.orientation);
            second.orientation = glm::normalize(glm::angleAxis(-angle * second.invInertia / angularSum, axis) * second.orientation);
            refreshCom(first);
            refreshCom(second);
        }

        void correctVelocityAxis(Slot& first, Slot& second, const Resting::Quantum& rest, vec3 axis) {
            const vec3 armA = worldOf(first.origin, first.orientation, rest.anchorFirst) - vec3{first.com};
            const vec3 armB = worldOf(second.origin, second.orientation, rest.anchorSecond) - vec3{second.com};
            const vec3 rel = (second.linear + glm::cross(second.omega, armB)) - (first.linear + glm::cross(first.omega, armA));
            const vec3 rxtA = glm::cross(armA, axis);
            const vec3 rxtB = glm::cross(armB, axis);
            const float weight = first.invMass + second.invMass + glm::dot(rxtA, rxtA) * first.invInertia + glm::dot(rxtB, rxtB) * second.invInertia;
            if (weight <= minLength)
                return;
            const float impulse = -glm::dot(rel, axis) / weight;
            first.linear -= axis * (impulse * first.invMass);
            second.linear += axis * (impulse * second.invMass);
            first.omega -= rxtA * (impulse * first.invInertia);
            second.omega += rxtB * (impulse * second.invInertia);
        }

        void correctVelocity(Slot& first, Slot& second, const Resting::Quantum& rest) {
            vec3 normal = first.orientation * rest.normalFirst;
            const float normalLength = glm::length(normal);
            normal = normalLength > minLength ? normal / normalLength : vec3{0.0f, 1.0f, 0.0f};
            const vec3 guide = glm::abs(normal.x) < 0.8f ? vec3{1.0f, 0.0f, 0.0f} : vec3{0.0f, 1.0f, 0.0f};
            const vec3 tangent = glm::normalize(glm::cross(normal, guide));
            correctVelocityAxis(first, second, rest, normal);
            correctVelocityAxis(first, second, rest, tangent);
            correctVelocityAxis(first, second, rest, glm::cross(normal, tangent));
            const float angularSum = first.invInertia + second.invInertia;
            if (angularSum <= 0.0f)
                return;
            const vec3 deltaOmega = second.omega - first.omega;
            first.omega += deltaOmega * (first.invInertia / angularSum);
            second.omega -= deltaOmega * (second.invInertia / angularSum);
        }

        auto slotMoved(const Slot& slot) -> bool {
            const float dt = float(Settings::fixedStep);
            return glm::length(vec3{slot.origin - slot.origin0}) > Settings::restLinear or rotationDistance(slot.orientation, slot.orientation0) > 1.0e-5f or glm::length(slot.linear - slot.linear0) * dt > Settings::restLinear or glm::length(slot.omega - slot.omega0) * dt > 1.0e-5f;
        }

        void applySlot(Slot& slot, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            if (not slotMoved(slot))
                return;
            auto* body = bodies.items.find(slot.id);
            if (not body)
                return;
            const quat turn = glm::normalize(slot.orientation * glm::conjugate(slot.orientation0));
            const dvec3 move = slot.origin - slot.origin0;
            if (auto* solid = solids.items.find(slot.id)) {
                body->position = slot.origin;
                body->orientation = slot.orientation;
                solid->center.position = slot.origin;
                solid->center.prev = slot.origin - dvec3{slot.linear} * Settings::fixedStep;
                const float angle = glm::length(slot.omega) * float(Settings::fixedStep);
                const quat qRel = angle > minLength ? glm::angleAxis(angle, slot.omega / glm::length(slot.omega)) : quat{1.0f, 0.0f, 0.0f, 0.0f};
                solid->prevOri = glm::normalize(glm::conjugate(qRel) * slot.orientation);
                return;
            }
            auto* crystal = crystals.items.find(slot.id);
            if (not crystal)
                return;
            const bool turned = rotationDistance(slot.orientation, slot.orientation0) > 1.0e-8f;
            const glm::dquat turn64{turn};
            const dvec3 com = slot.origin + dvec3{slot.orientation * slot.localCom};
            const vec3 dLinear = slot.linear - slot.linear0;
            const vec3 dOmega = slot.omega - slot.omega0;
            const bool kickVelocity = glm::dot(dLinear, dLinear) + glm::dot(dOmega, dOmega) > minLength;
            for (Particle& particle : crystal->particles) {
                if (turned) {
                    particle.position = slot.origin + turn64 * (particle.position - slot.origin0);
                    particle.prev = slot.origin + turn64 * (particle.prev - slot.origin0);
                } else {
                    particle.position += move;
                    particle.prev += move;
                }
                if (kickVelocity)
                    particle.prev -= dvec3{dLinear + glm::cross(dOmega, vec3{particle.position - com})} * Settings::fixedStep;
            }
            body->position = slot.origin;
            body->orientation = slot.orientation;
        }

        void censusProbes(State& state) {
            state.census.restProbes = static_cast<integer>(state.probes.size());
            seconds oldest{0};
            for (const auto& [_, probe] : state.probes)
                oldest = glm::max(oldest, probe.stable);
            state.census.restProbeStable = oldest;
        }

        void countIslands(State& state) {
            state.census.restingIslands = 0;
            std::unordered_map<Body::Id, vector<Body::Id>> graph;
            for (const PairKey& pair : state.activeResting) {
                graph[pair.first].push_back(pair.second);
                graph[pair.second].push_back(pair.first);
            }
            std::unordered_set<Body::Id> walked;
            vector<Body::Id> walk;
            for (const auto& [body, _] : graph) {
                if (walked.contains(body))
                    continue;
                ++state.census.restingIslands;
                walk.clear();
                walk.push_back(body);
                walked.insert(body);
                for (std::size_t head = 0; head < walk.size(); ++head) {
                    for (const Body::Id next : graph[walk[head]]) {
                        if (walked.insert(next).second)
                            walk.push_back(next);
                    }
                }
            }
        }

        void removeResting(Stewarding context, const vector<Resting::Id>& stale) {
            for (const Resting::Id id : stale)
                with<Resting>::remove(context, id);
        }

        auto collectLive(State& state, Stewarding context) -> vector<Resting::Id> {
            vector<Resting::Id> live;
            for (auto [id, rest] : context.direct<Resting>().items) {
                if (state.activeResting.contains(pairKey(rest.first, rest.second)))
                    live.push_back(id);
            }
            return live;
        }

        void solveIsland(const vector<Resting::Id>& links, Stewarding context, fqsm::Direct<Body> bodies, fqsm::Direct<Solid> solids, fqsm::Direct<Crystal> crystals) {
            std::unordered_map<Body::Id, integer> indexOf;
            vector<Slot> slots;
            auto take = [&](Body::Id id) {
                if (indexOf.contains(id))
                    return;
                auto* body = bodies.items.find(id);
                if (not body)
                    return;
                indexOf.emplace(id, static_cast<integer>(slots.size()));
                slots.push_back(readSlot(id, *body, solids, crystals));
            };
            auto restings = context.direct<Resting>();
            for (const Resting::Id id : links) {
                auto* rest = restings.items.find(id);
                if (not rest)
                    continue;
                take(rest->first);
                take(rest->second);
            }
            if (slots.empty())
                return;
            for (integer iteration = 0; iteration < Settings::Resting::solveIterations; ++iteration) {
                for (const Resting::Id id : links) {
                    auto* rest = restings.items.find(id);
                    if (not rest)
                        continue;
                    auto foundA = indexOf.find(rest->first);
                    auto foundB = indexOf.find(rest->second);
                    if (foundA == indexOf.end() or foundB == indexOf.end())
                        continue;
                    correctPose(slots[static_cast<std::size_t>(foundA->second)], slots[static_cast<std::size_t>(foundB->second)], *rest);
                }
            }
            for (integer iteration = 0; iteration < Settings::Resting::solveIterations; ++iteration) {
                for (const Resting::Id id : links) {
                    auto* rest = restings.items.find(id);
                    if (not rest)
                        continue;
                    auto foundA = indexOf.find(rest->first);
                    auto foundB = indexOf.find(rest->second);
                    if (foundA == indexOf.end() or foundB == indexOf.end())
                        continue;
                    correctVelocity(slots[static_cast<std::size_t>(foundA->second)], slots[static_cast<std::size_t>(foundB->second)], *rest);
                }
            }
            for (Slot& slot : slots)
                applySlot(slot, bodies, solids, crystals);
        }

    }

    auto pairKey(Body::Id first, Body::Id second) -> PairKey {
        return first.raw() < second.raw() ? PairKey{first, second} : PairKey{second, first};
    }

    void prepareResting(State& state, Stewarding context) {
        state.activeResting.clear();
        if (not Settings::Resting::enabled) {
            vector<Resting::Id> stale;
            for (auto [id, _] : context.direct<Resting>().items)
                stale.push_back(id);
            removeResting(context, stale);
            state.probes.clear();
            state.census.restingPairs = 0;
            state.census.restingIslands = 0;
            censusProbes(state);
            return;
        }
        auto bodies = context.direct<Body>();
        auto solids = context.direct<Solid>();
        auto crystals = context.direct<Crystal>();
        vector<Resting::Id> stale;
        for (auto [id, rest] : context.direct<Resting>().items) {
            auto* first = bodies.items.find(rest.first);
            auto* second = bodies.items.find(rest.second);
            if (not first or not second or not sameGeometry(rest, *first, *second, solids, crystals)) {
                stale.push_back(id);
                continue;
            }
            const RestError error = restError(rest, first->position, first->orientation, second->position, second->orientation, inverseMass(first->totalMass), inverseMass(second->totalMass));
            if (breaksRest(rest, error) or not state.activeResting.insert(pairKey(rest.first, rest.second)).second) {
                stale.push_back(id);
                continue;
            }
            rest.normalLoad = glm::max(rest.normalLoad * 0.9f, error.compressionImpulse);
        }
        removeResting(context, stale);
        countIslands(state);
        state.census.restingPairs = static_cast<integer>(state.activeResting.size());
        censusProbes(state);
    }

    void recheckResting(State& state, Stewarding context) {
        auto bodies = context.direct<Body>();
        vector<Resting::Id> stale;
        for (auto [id, rest] : context.direct<Resting>().items) {
            if (not state.activeResting.contains(pairKey(rest.first, rest.second)))
                continue;
            auto* first = bodies.items.find(rest.first);
            auto* second = bodies.items.find(rest.second);
            if (not first or not second) {
                stale.push_back(id);
                continue;
            }
            const RestError error = restError(rest, first->position, first->orientation, second->position, second->orientation, inverseMass(first->totalMass), inverseMass(second->totalMass));
            if (breaksRest(rest, error))
                stale.push_back(id);
        }
        if (stale.empty())
            return;
        auto restings = context.direct<Resting>();
        for (const Resting::Id id : stale) {
            if (auto* rest = restings.items.find(id))
                state.activeResting.erase(pairKey(rest->first, rest->second));
        }
        removeResting(context, stale);
        countIslands(state);
        state.census.restingPairs = static_cast<integer>(state.activeResting.size());
    }

    void acquireResting(State& state, Stewarding context) {
        if (not Settings::Resting::enabled) {
            state.probes.clear();
            censusProbes(state);
            return;
        }
        struct Aggregate {
            vec3 point;
            vec3 normal;
            float weight;
        };
        std::unordered_map<PairKey, Aggregate, PairKeyHash> aggregates;
        for (const Contact& contact : state.contacts) {
            if (contact.penetration <= 0.0f)
                continue;
            const PairKey pair = pairKey(contact.a.body, contact.b.body);
            if (state.activeResting.contains(pair))
                continue;
            const bool forward = contact.a.body == pair.first;
            const float weight = glm::max(contact.penetration, minLength);
            const vec3 normal = forward ? contact.normal : -contact.normal;
            auto found = aggregates.find(pair);
            if (found == aggregates.end()) {
                aggregates.emplace(pair, Aggregate{contact.point * weight, normal * weight, weight});
                continue;
            }
            found->second.point += contact.point * weight;
            found->second.normal += normal * weight;
            found->second.weight += weight;
        }
        auto bodies = context.direct<Body>();
        auto solids = context.direct<Solid>();
        auto crystals = context.direct<Crystal>();
        std::unordered_map<PairKey, RestProbe, PairKeyHash> nextProbes;
        nextProbes.reserve(aggregates.size());
        for (const auto& [pair, aggregate] : aggregates) {
            auto* first = bodies.items.find(pair.first);
            auto* second = bodies.items.find(pair.second);
            if (not first or not second or aggregate.weight <= 0.0f)
                continue;
            const integer firstShape = shapeSignature(pair.first, solids, crystals);
            const integer secondShape = shapeSignature(pair.second, solids, crystals);
            if (firstShape == 0 or secondShape == 0)
                continue;
            const vec3 point = aggregate.point / aggregate.weight;
            const float normalLength = glm::length(aggregate.normal);
            if (normalLength <= minLength)
                continue;
            const vec3 normal = aggregate.normal / normalLength;
            const vec3 localFirst = glm::conjugate(first->orientation) * (point - vec3{first->position});
            const vec3 localSecond = glm::conjugate(second->orientation) * (point - vec3{second->position});
            const quat relativeOrientation = glm::normalize(glm::conjugate(first->orientation) * second->orientation);
            auto previous = state.probes.find(pair);
            const bool continuing = previous != state.probes.end() and previous->second.firstShape == firstShape and previous->second.secondShape == secondShape and glm::length(localFirst - previous->second.localFirst) <= Settings::Resting::captureMeters and glm::length(localSecond - previous->second.localSecond) <= Settings::Resting::captureMeters and rotationDistance(relativeOrientation, previous->second.relativeOrientation) <= Settings::Resting::captureRadians;
            const RestProbe probe{
                .localFirst = continuing ? previous->second.localFirst : localFirst,
                .localSecond = continuing ? previous->second.localSecond : localSecond,
                .relativeOrientation = continuing ? previous->second.relativeOrientation : relativeOrientation,
                .firstShape = firstShape,
                .secondShape = secondShape,
                .stable = continuing ? previous->second.stable + Settings::fixedStep : Settings::fixedStep,
            };
            if (float(probe.stable) < Settings::Resting::captureSeconds) {
                nextProbes.emplace(pair, probe);
                continue;
            }
            const vec3 originOffset = glm::conjugate(first->orientation) * vec3{second->position - first->position};
            with<Resting>::create(context, Resting::Quantum{.first = pair.first, .second = pair.second, .anchorFirst = localFirst, .anchorSecond = localSecond, .normalFirst = glm::conjugate(first->orientation) * normal, .relativeOffset = originOffset, .relativeOrientation = relativeOrientation, .normalLoad = 0.0f, .firstRadius = first->radius, .secondRadius = second->radius, .firstShape = firstShape, .secondShape = secondShape});
            state.activeResting.insert(pair);
        }
        state.probes = std::move(nextProbes);
        countIslands(state);
        state.census.restingPairs = static_cast<integer>(state.activeResting.size());
        censusProbes(state);
    }

    void solveRestingIslands(State& state, Stewarding context) {
        const auto live = collectLive(state, context);
        if (live.empty())
            return;
        std::unordered_map<Body::Id, vector<Resting::Id>> graph;
        auto restings = context.direct<Resting>();
        for (const Resting::Id id : live) {
            auto* rest = restings.items.find(id);
            if (not rest)
                continue;
            graph[rest->first].push_back(id);
            graph[rest->second].push_back(id);
        }
        std::unordered_set<Resting::Id> seen;
        vector<Resting::Id> island;
        auto bodies = context.direct<Body>();
        auto solids = context.direct<Solid>();
        auto crystals = context.direct<Crystal>();
        for (const Resting::Id seed : live) {
            if (seen.contains(seed))
                continue;
            island.clear();
            vector<Resting::Id> walk{seed};
            seen.insert(seed);
            for (std::size_t head = 0; head < walk.size(); ++head) {
                const Resting::Id id = walk[head];
                island.push_back(id);
                auto* rest = restings.items.find(id);
                if (not rest)
                    continue;
                const Body::Id ends[2] = {rest->first, rest->second};
                for (const Body::Id body : ends) {
                    auto found = graph.find(body);
                    if (found == graph.end())
                        continue;
                    for (const Resting::Id next : found->second) {
                        if (seen.insert(next).second)
                            walk.push_back(next);
                    }
                }
            }
            solveIsland(island, context, bodies, solids, crystals);
        }
    }

}
