#include "physics/collisions.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <unordered_map>

namespace eltanin::phys::collision {

    using rigid::Ball;
    using rigid::Crystal;
    using rigid::Hull;

    namespace {

        constexpr float minLength = 1.0e-8f;
        constexpr int solverPasses = 4;
        constexpr float solverRate = 0.5f;
        constexpr float clusterAlign = 0.75f;

        struct Occupant {
            Endpoint::Type type;
            Body::Id body;
            vec3 center;
            float radius;
        };

        struct Cohort {
            Compound::Id id;
            vec3 center;
            float radius;
            vector<Occupant> occupants;
        };

        auto closestPointOnTriangle(vec3 point, vec3 corner0, vec3 corner1, vec3 corner2) -> vec3 {
            const vec3 edge01 = corner1 - corner0;
            const vec3 edge02 = corner2 - corner0;
            const vec3 toPoint0 = point - corner0;
            const float dot01 = glm::dot(edge01, toPoint0);
            const float dot02 = glm::dot(edge02, toPoint0);
            if (dot01 <= 0.0f and dot02 <= 0.0f)
                return corner0;
            const vec3 toPoint1 = point - corner1;
            const float dot11 = glm::dot(edge01, toPoint1);
            const float dot12 = glm::dot(edge02, toPoint1);
            if (dot11 >= 0.0f and dot12 <= dot11)
                return corner1;
            const float regionEdge01 = dot01 * dot12 - dot11 * dot02;
            if (regionEdge01 <= 0.0f and dot01 >= 0.0f and dot11 <= 0.0f)
                return corner0 + (dot01 / (dot01 - dot11)) * edge01;
            const vec3 toPoint2 = point - corner2;
            const float dot21 = glm::dot(edge01, toPoint2);
            const float dot22 = glm::dot(edge02, toPoint2);
            if (dot22 >= 0.0f and dot21 <= dot22)
                return corner2;
            const float regionEdge02 = dot21 * dot02 - dot01 * dot22;
            if (regionEdge02 <= 0.0f and dot02 >= 0.0f and dot22 <= 0.0f)
                return corner0 + (dot02 / (dot02 - dot22)) * edge02;
            const float regionEdge12 = dot11 * dot22 - dot21 * dot12;
            if (regionEdge12 <= 0.0f and (dot12 - dot11) >= 0.0f and (dot21 - dot22) >= 0.0f)
                return corner1 + ((dot12 - dot11) / ((dot12 - dot11) + (dot21 - dot22))) * (corner2 - corner1);
            const float denom = 1.0f / (regionEdge12 + regionEdge02 + regionEdge01);
            return corner0 + edge01 * (regionEdge02 * denom) + edge02 * (regionEdge01 * denom);
        }

        auto worldOf(const Body::Quantum& body, vec3 local) -> vec3 {
            return body.position + body.orientation * local;
        }

        auto faceUsable(const Crystal::Quantum& crystal, const Hull::Face& face) -> bool {
            return face.points.size() >= 3
                and static_cast<std::size_t>(face.points[0]) < crystal.shape.size()
                and static_cast<std::size_t>(face.points[1]) < crystal.shape.size()
                and static_cast<std::size_t>(face.points[2]) < crystal.shape.size();
        }

        void addOccupant(vector<Occupant>& occupants, Body::Id id, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            auto* body = bodies.items.find(id);
            if (not body or body->radius <= 0.0f or body->mass <= 0.0f)
                return;
            if (balls.items.find(id)) {
                occupants.push_back(Occupant{Endpoint::Type::ball, id, body->position, body->radius});
                return;
            }
            auto* crystal = crystals.items.find(id);
            if (not crystal)
                return;
            occupants.push_back(Occupant{Endpoint::Type::crystal, id, worldOf(*body, crystal->com), body->radius});
        }

        auto velocityOf(Body::Id id, Endpoint::Type type, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) -> vec3 {
            if (type == Endpoint::Type::ball) {
                auto* body = bodies.items.find(id);
                auto* ball = balls.items.find(id);
                if (not body or not ball)
                    return vec3{0.0f, 0.0f, 0.0f};
                return (body->position - ball->prevPos) / Particle::dt;
            }
            auto* crystal = crystals.items.find(id);
            if (not crystal or crystal->particles.empty())
                return vec3{0.0f, 0.0f, 0.0f};
            vec3 sum{0.0f, 0.0f, 0.0f};
            for (const Particle& particle : crystal->particles)
                sum += particle.velocity();
            return sum / static_cast<float>(crystal->particles.size());
        }

        auto spheresOverlap(vec3 centerA, float radiusA, vec3 centerB, float radiusB) -> bool {
            const vec3 offset = centerB - centerA;
            const float limit = radiusA + radiusB;
            return glm::dot(offset, offset) <= limit * limit;
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

        void contactBallBall(State& state, integer candidate, const Occupant& first, const Occupant& second, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            const vec3 offset = second.center - first.center;
            const float distance = glm::length(offset);
            const float penetration = first.radius + second.radius - distance;
            const vec3 fromFirstTowardSecond = distance >= minLength ? offset : vec3{1.0f, 0.0f, 0.0f};
            const vec3 point = first.center + glm::normalize(fromFirstTowardSecond) * first.radius;
            pushContact(state, candidate, Endpoint{Endpoint::Type::ball, first.body, 0}, Endpoint{Endpoint::Type::ball, second.body, 0}, point, fromFirstTowardSecond, penetration, velocityOf(first.body, first.type, bodies, balls, crystals), velocityOf(second.body, second.type, bodies, balls, crystals));
        }

        void contactBallCrystal(State& state, integer candidate, const Occupant& ball, const Occupant& crystalOccupant, bool ballIsFirst, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            auto* body = bodies.items.find(crystalOccupant.body);
            auto* crystal = crystals.items.find(crystalOccupant.body);
            if (not body or not crystal)
                return;
            const vec3 ballVelocity = velocityOf(ball.body, ball.type, bodies, balls, crystals);
            const vec3 crystalVelocity = velocityOf(crystalOccupant.body, crystalOccupant.type, bodies, balls, crystals);
            for (std::size_t faceIndex = 0; faceIndex < crystal->hull.faces.size(); ++faceIndex) {
                const auto& face = crystal->hull.faces[faceIndex];
                if (not faceUsable(*crystal, face))
                    continue;
                const vec3 corner0 = worldOf(*body, crystal->shape[static_cast<std::size_t>(face.points[0])]);
                const vec3 corner1 = worldOf(*body, crystal->shape[static_cast<std::size_t>(face.points[1])]);
                const vec3 corner2 = worldOf(*body, crystal->shape[static_cast<std::size_t>(face.points[2])]);
                const vec3 closest = closestPointOnTriangle(ball.center, corner0, corner1, corner2);
                const vec3 offset = ball.center - closest;
                const float distance = glm::length(offset);
                const float penetration = ball.radius - distance;
                if (penetration <= 0.0f)
                    continue;
                const vec3 fromCrystalTowardBall = distance >= minLength ? offset : body->orientation * face.normal;
                const Endpoint ballEnd{Endpoint::Type::ball, ball.body, 0};
                const Endpoint crystalEnd{Endpoint::Type::crystal, crystalOccupant.body, static_cast<integer>(faceIndex)};
                if (ballIsFirst)
                    pushContact(state, candidate, ballEnd, crystalEnd, closest, -fromCrystalTowardBall, penetration, ballVelocity, crystalVelocity);
                else
                    pushContact(state, candidate, crystalEnd, ballEnd, closest, fromCrystalTowardBall, penetration, crystalVelocity, ballVelocity);
            }
        }

        void contactCrystalVertexFaces(State& state, integer candidate, const Occupant& vertexSide, const Occupant& faceSide, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            auto* vertexBody = bodies.items.find(vertexSide.body);
            auto* vertexCrystal = crystals.items.find(vertexSide.body);
            auto* faceBody = bodies.items.find(faceSide.body);
            auto* faceCrystal = crystals.items.find(faceSide.body);
            if (not vertexBody or not vertexCrystal or not faceBody or not faceCrystal)
                return;
            const vec3 velocityVertex = velocityOf(vertexSide.body, vertexSide.type, bodies, balls, crystals);
            const vec3 velocityFace = velocityOf(faceSide.body, faceSide.type, bodies, balls, crystals);
            for (std::size_t vertexIndex = 0; vertexIndex < vertexCrystal->shape.size(); ++vertexIndex) {
                const vec3 vertex = worldOf(*vertexBody, vertexCrystal->shape[vertexIndex]);
                for (std::size_t faceIndex = 0; faceIndex < faceCrystal->hull.faces.size(); ++faceIndex) {
                    const auto& face = faceCrystal->hull.faces[faceIndex];
                    if (not faceUsable(*faceCrystal, face))
                        continue;
                    const vec3 corner0 = worldOf(*faceBody, faceCrystal->shape[static_cast<std::size_t>(face.points[0])]);
                    const vec3 corner1 = worldOf(*faceBody, faceCrystal->shape[static_cast<std::size_t>(face.points[1])]);
                    const vec3 corner2 = worldOf(*faceBody, faceCrystal->shape[static_cast<std::size_t>(face.points[2])]);
                    const vec3 closest = closestPointOnTriangle(vertex, corner0, corner1, corner2);
                    const vec3 rotated = faceBody->orientation * face.normal;
                    const float normalLength = glm::length(rotated);
                    if (normalLength < minLength)
                        continue;
                    const vec3 worldNormal = rotated / normalLength;
                    const float signedDistance = glm::dot(vertex - corner0, worldNormal);
                    if (signedDistance >= 0.0f)
                        continue;
                    const vec3 delta = vertex - closest;
                    if (glm::dot(delta, delta) > signedDistance * signedDistance + 1.0e-6f)
                        continue;
                    pushContact(state, candidate, Endpoint{Endpoint::Type::crystal, vertexSide.body, static_cast<integer>(vertexIndex)}, Endpoint{Endpoint::Type::crystal, faceSide.body, static_cast<integer>(faceIndex)}, closest, -worldNormal, -signedDistance, velocityVertex, velocityFace);
                }
            }
        }

        auto collectOccupants(Compound::Id host, const Compound::Quantum& compound, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) -> vector<Occupant> {
            vector<Occupant> occupants;
            addOccupant(occupants, host, bodies, balls, crystals);
            for (const Body::Id member : compound.members) {
                if (member == host)
                    continue;
                addOccupant(occupants, member, bodies, balls, crystals);
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
                radius = std::max(radius, glm::length(occupant.center - center) + occupant.radius);
            return true;
        }

        auto indexOf(std::unordered_map<Body::Id, integer>& indices, vector<Body::Id>& bodies, Body::Id id) -> integer {
            auto found = indices.find(id);
            if (found != indices.end())
                return found->second;
            const integer index = static_cast<integer>(bodies.size());
            bodies.push_back(id);
            indices.emplace(id, index);
            return index;
        }

        auto rootOf(vector<integer>& parent, integer index) -> integer {
            integer root = index;
            while (parent[static_cast<std::size_t>(root)] != root)
                root = parent[static_cast<std::size_t>(root)];
            integer walk = index;
            while (parent[static_cast<std::size_t>(walk)] != root) {
                const integer next = parent[static_cast<std::size_t>(walk)];
                parent[static_cast<std::size_t>(walk)] = root;
                walk = next;
            }
            return root;
        }

        auto measureBallBall(const Body::Quantum& first, const Body::Quantum& second, vec3& point, vec3& normal) -> float {
            const vec3 offset = second.position - first.position;
            const float distance = glm::length(offset);
            const vec3 fromFirstTowardSecond = distance >= minLength ? offset / distance : vec3{1.0f, 0.0f, 0.0f};
            point = first.position + fromFirstTowardSecond * first.radius;
            normal = fromFirstTowardSecond;
            return first.radius + second.radius - distance;
        }

        auto measureBallFace(const Body::Quantum& ball, const Body::Quantum& crystalBody, const Crystal::Quantum& crystal, integer faceIndex, bool ballIsFirst, vec3& point, vec3& normal) -> float {
            if (faceIndex < 0 or static_cast<std::size_t>(faceIndex) >= crystal.hull.faces.size())
                return 0.0f;
            const auto& face = crystal.hull.faces[static_cast<std::size_t>(faceIndex)];
            if (not faceUsable(crystal, face))
                return 0.0f;
            const vec3 corner0 = worldOf(crystalBody, crystal.shape[static_cast<std::size_t>(face.points[0])]);
            const vec3 corner1 = worldOf(crystalBody, crystal.shape[static_cast<std::size_t>(face.points[1])]);
            const vec3 corner2 = worldOf(crystalBody, crystal.shape[static_cast<std::size_t>(face.points[2])]);
            const vec3 closest = closestPointOnTriangle(ball.position, corner0, corner1, corner2);
            const vec3 offset = ball.position - closest;
            const float distance = glm::length(offset);
            const float penetration = ball.radius - distance;
            if (penetration <= 0.0f)
                return 0.0f;
            const vec3 fromCrystalTowardBall = distance >= minLength ? offset / distance : glm::normalize(crystalBody.orientation * face.normal);
            point = closest;
            normal = ballIsFirst ? -fromCrystalTowardBall : fromCrystalTowardBall;
            return penetration;
        }

        auto measureVertexFace(const Body::Quantum& vertexBody, const Crystal::Quantum& vertexCrystal, integer vertexIndex, const Body::Quantum& faceBody, const Crystal::Quantum& faceCrystal, integer faceIndex, vec3& point, vec3& normal) -> float {
            if (vertexIndex < 0 or static_cast<std::size_t>(vertexIndex) >= vertexCrystal.shape.size())
                return 0.0f;
            if (faceIndex < 0 or static_cast<std::size_t>(faceIndex) >= faceCrystal.hull.faces.size())
                return 0.0f;
            const auto& face = faceCrystal.hull.faces[static_cast<std::size_t>(faceIndex)];
            if (not faceUsable(faceCrystal, face))
                return 0.0f;
            const vec3 vertex = worldOf(vertexBody, vertexCrystal.shape[static_cast<std::size_t>(vertexIndex)]);
            const vec3 corner0 = worldOf(faceBody, faceCrystal.shape[static_cast<std::size_t>(face.points[0])]);
            const vec3 corner1 = worldOf(faceBody, faceCrystal.shape[static_cast<std::size_t>(face.points[1])]);
            const vec3 corner2 = worldOf(faceBody, faceCrystal.shape[static_cast<std::size_t>(face.points[2])]);
            const vec3 closest = closestPointOnTriangle(vertex, corner0, corner1, corner2);
            const vec3 rotated = faceBody.orientation * face.normal;
            const float normalLength = glm::length(rotated);
            if (normalLength < minLength)
                return 0.0f;
            const vec3 worldNormal = rotated / normalLength;
            const float signedDistance = glm::dot(vertex - corner0, worldNormal);
            if (signedDistance >= 0.0f)
                return 0.0f;
            const vec3 delta = vertex - closest;
            if (glm::dot(delta, delta) > signedDistance * signedDistance + 1.0e-6f)
                return 0.0f;
            point = closest;
            normal = -worldNormal;
            return -signedDistance;
        }

        auto remeasure(Contact& contact, fqsm::Direct<Body> bodies, fqsm::Direct<Crystal> crystals) -> float {
            auto* bodyA = bodies.items.find(contact.a.body);
            auto* bodyB = bodies.items.find(contact.b.body);
            if (not bodyA or not bodyB)
                return 0.0f;
            if (contact.a.type == Endpoint::Type::ball and contact.b.type == Endpoint::Type::ball)
                return measureBallBall(*bodyA, *bodyB, contact.point, contact.normal);
            if (contact.a.type == Endpoint::Type::ball and contact.b.type == Endpoint::Type::crystal) {
                auto* crystal = crystals.items.find(contact.b.body);
                if (not crystal)
                    return 0.0f;
                return measureBallFace(*bodyA, *bodyB, *crystal, contact.b.face, true, contact.point, contact.normal);
            }
            if (contact.a.type == Endpoint::Type::crystal and contact.b.type == Endpoint::Type::ball) {
                auto* crystal = crystals.items.find(contact.a.body);
                if (not crystal)
                    return 0.0f;
                return measureBallFace(*bodyB, *bodyA, *crystal, contact.a.face, false, contact.point, contact.normal);
            }
            auto* crystalA = crystals.items.find(contact.a.body);
            auto* crystalB = crystals.items.find(contact.b.body);
            if (not crystalA or not crystalB)
                return 0.0f;
            return measureVertexFace(*bodyA, *crystalA, contact.a.face, *bodyB, *crystalB, contact.b.face, contact.point, contact.normal);
        }

        void reduceManifold(State& state) {
            if (state.contacts.size() < 2)
                return;
            std::sort(state.contacts.begin(), state.contacts.end(), [](const Contact& left, const Contact& right) {
                return left.penetration > right.penetration;
            });
            vector<Contact> kept;
            kept.reserve(state.contacts.size());
            for (const Contact& contact : state.contacts) {
                bool clustered = false;
                for (const Contact& existing : kept) {
                    if (existing.a.body != contact.a.body or existing.b.body != contact.b.body)
                        continue;
                    if (glm::dot(existing.normal, contact.normal) < clusterAlign)
                        continue;
                    clustered = true;
                    break;
                }
                if (not clustered)
                    kept.push_back(contact);
            }
            state.contacts.swap(kept);
        }

        void pairOccupants(State& state, const Occupant& first, const Occupant& second, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            if (first.body == second.body)
                return;
            if (not spheresOverlap(first.center, first.radius, second.center, second.radius))
                return;
            const integer candidate = static_cast<integer>(state.candidates.size());
            state.candidates.push_back(Candidate{
                .a = Endpoint{first.type, first.body, 0},
                .b = Endpoint{second.type, second.body, 0},
            });
            if (first.type == Endpoint::Type::ball and second.type == Endpoint::Type::ball)
                contactBallBall(state, candidate, first, second, bodies, balls, crystals);
            else if (first.type == Endpoint::Type::ball and second.type == Endpoint::Type::crystal)
                contactBallCrystal(state, candidate, first, second, true, bodies, balls, crystals);
            else if (first.type == Endpoint::Type::crystal and second.type == Endpoint::Type::ball)
                contactBallCrystal(state, candidate, second, first, false, bodies, balls, crystals);
            else {
                contactCrystalVertexFaces(state, candidate, first, second, bodies, balls, crystals);
                contactCrystalVertexFaces(state, candidate, second, first, bodies, balls, crystals);
            }
        }

        void formIslands(State& state) {
            state.islands.clear();
            if (state.contacts.empty())
                return;
            vector<Body::Id> bodies;
            std::unordered_map<Body::Id, integer> indices;
            for (const Contact& contact : state.contacts) {
                indexOf(indices, bodies, contact.a.body);
                indexOf(indices, bodies, contact.b.body);
            }
            vector<integer> parent(bodies.size());
            for (std::size_t index = 0; index < parent.size(); ++index)
                parent[index] = static_cast<integer>(index);
            for (const Contact& contact : state.contacts) {
                const integer first = rootOf(parent, indices.at(contact.a.body));
                const integer second = rootOf(parent, indices.at(contact.b.body));
                if (first != second)
                    parent[static_cast<std::size_t>(second)] = first;
            }
            for (std::size_t index = 0; index < parent.size(); ++index)
                (void)rootOf(parent, static_cast<integer>(index));
            std::sort(state.contacts.begin(), state.contacts.end(), [&](const Contact& left, const Contact& right) {
                return parent[static_cast<std::size_t>(indices.at(left.a.body))] < parent[static_cast<std::size_t>(indices.at(right.a.body))];
            });
            integer currentRoot = parent[static_cast<std::size_t>(indices.at(state.contacts.front().a.body))];
            integer begin = 0;
            for (integer index = 1; index < static_cast<integer>(state.contacts.size()); ++index) {
                const integer root = parent[static_cast<std::size_t>(indices.at(state.contacts[static_cast<std::size_t>(index)].a.body))];
                if (root == currentRoot)
                    continue;
                state.islands.push_back(Island{.contactBegin = begin, .contactEnd = index});
                begin = index;
                currentRoot = root;
            }
            state.islands.push_back(Island{.contactBegin = begin, .contactEnd = static_cast<integer>(state.contacts.size())});
        }

    }

    void State::build(Stewarding context) {
        candidates.clear();
        contacts.clear();
        islands.clear();
        auto bodies = context.direct<Body>();
        auto balls = context.direct<Ball>();
        auto crystals = context.direct<Crystal>();
        auto compounds = context.direct<Compound>();
        vector<Cohort> cohorts;
        for (auto [id, compound] : compounds.items) {
            auto occupants = collectOccupants(id, compound, bodies, balls, crystals);
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
                        pairOccupants(*this, occupantA, occupantB, bodies, balls, crystals);
                }
            }
        }
        reduceManifold(*this);
        formIslands(*this);
    }

    void State::solve(Stewarding context) {
        if (contacts.empty())
            return;
        auto bodies = context.direct<Body>();
        auto crystals = context.direct<Crystal>();
        for (int pass = 0; pass < solverPasses; ++pass) {
            for (Contact& contact : contacts) {
                const float remaining = remeasure(contact, bodies, crystals);
                if (remaining <= 0.0f)
                    continue;
                auto* bodyA = bodies.items.find(contact.a.body);
                auto* bodyB = bodies.items.find(contact.b.body);
                if (not bodyA or not bodyB)
                    continue;
                const float weightA = bodyA->mass > 0.0f ? 1.0f / bodyA->mass : 0.0f;
                const float weightB = bodyB->mass > 0.0f ? 1.0f / bodyB->mass : 0.0f;
                const float weightSum = weightA + weightB;
                if (weightSum <= 0.0f)
                    continue;
                const float step = remaining * solverRate;
                bodyA->position -= contact.normal * (step * (weightA / weightSum));
                bodyB->position += contact.normal * (step * (weightB / weightSum));
                contact.correction += step;
                contact.penetration = remaining;
            }
        }
    }

}
