#include "physics/collisions.h"
#include "physics/hullBvh.h"

#include <base/logging.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eltanin::phys::collision {

    using rigid::Ball;
    using rigid::Crystal;

    namespace {

        constexpr float minLength = 1.0e-8f;
        constexpr float ballFriction = 0.8f;

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

        void addOccupant(vector<Occupant>& occupants, Body::Id id, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            auto* body = bodies.items.find(id);
            if (not body or body->radius <= 0.0f or body->mass <= 0.0f)
                return;
            if (balls.items.find(id)) {
                occupants.push_back(Occupant{Endpoint::Type::ball, id, vec3{body->position}, body->radius});
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
                return vec3{(body->position - ball->prevPos) / double(Particle::dt)};
            }
            auto* crystal = crystals.items.find(id);
            if (not crystal or crystal->particles.empty())
                return vec3{0.0f, 0.0f, 0.0f};
            dvec3 sum{0.0, 0.0, 0.0};
            for (const Particle& particle : crystal->particles)
                sum += particle.velocity();
            return vec3{sum / double(crystal->particles.size())};
        }

        auto omegaOf(const Body::Quantum& body, const Ball::Quantum& ball) -> vec3 {
            const float dt = Particle::dt;
            const quat qRel = glm::normalize(body.orientation * glm::conjugate(ball.prevOri));
            vec3 omega = (2.0f / dt) * vec3{qRel.x, qRel.y, qRel.z};
            if (qRel.w < 0.0f)
                omega = -omega;
            return omega;
        }

        auto velocityAt(const Body::Quantum& body, const Ball::Quantum& ball, vec3 worldPoint) -> vec3 {
            const vec3 linear = vec3{(body.position - ball.prevPos) / double(Particle::dt)};
            return linear + glm::cross(omegaOf(body, ball), worldPoint - vec3{body.position});
        }

        auto sphereInertia(const Body::Quantum& body) -> float {
            return 0.4f * body.mass * body.radius * body.radius;
        }

        auto spinWeight(const Body::Quantum& body, vec3 arm, vec3 tangent) -> float {
            const float inertia = sphereInertia(body);
            const vec3 rxt = glm::cross(arm, tangent);
            return inverseMass(body.mass) + (inertia > 1.0e-12f ? glm::dot(rxt, rxt) / inertia : 0.0f);
        }

        void kickBall(Body::Quantum& body, vec3 arm, vec3 impulse) {
            if (body.mass <= 0.0f)
                return;
            body.position += dvec3{impulse / body.mass};
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

        void contactParticlesVsShape(State& state, integer candidate, const Occupant& particleSide, const Occupant& shapeSide, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            auto* particleCrystal = crystals.items.find(particleSide.body);
            auto* shapeBody = bodies.items.find(shapeSide.body);
            auto* shapeCrystal = crystals.items.find(shapeSide.body);
            if (not particleCrystal or not shapeBody or not shapeCrystal)
                return;
            if (particleCrystal->particles.empty() or particleCrystal->particles.size() != particleCrystal->shape.size())
                return;
            const vec3 shapeVelocity = velocityOf(shapeSide.body, shapeSide.type, bodies, balls, crystals);
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

        void contactBallVsShape(State& state, integer candidate, const Occupant& ball, const Occupant& shapeSide, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            auto* shapeBody = bodies.items.find(shapeSide.body);
            auto* shapeCrystal = crystals.items.find(shapeSide.body);
            if (not shapeBody or not shapeCrystal)
                return;
            integer faceIndex = -1;
            vec3 closest;
            vec3 outward;
            const float depth = nearestSurface(*shapeBody, *shapeCrystal, ball.center, ball.radius, faceIndex, closest, outward);
            if (depth <= 0.0f)
                return;
            pushContact(state, candidate, Endpoint{Endpoint::Type::ball, ball.body, 0}, Endpoint{Endpoint::Type::crystal, shapeSide.body, faceIndex}, closest, -outward, depth, velocityOf(ball.body, ball.type, bodies, balls, crystals), velocityOf(shapeSide.body, shapeSide.type, bodies, balls, crystals));
        }

        void contactParticlesVsBall(State& state, integer candidate, const Occupant& particleSide, const Occupant& ball, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            auto* particleCrystal = crystals.items.find(particleSide.body);
            if (not particleCrystal or particleCrystal->particles.empty())
                return;
            const vec3 ballVelocity = velocityOf(ball.body, ball.type, bodies, balls, crystals);
            const vec3 center = ball.center;
            for (std::size_t vertexIndex = 0; vertexIndex < particleCrystal->particles.size(); ++vertexIndex) {
                const vec3 vertex = vec3{particleCrystal->particles[vertexIndex].position};
                const vec3 offset = vertex - center;
                const float distance = glm::length(offset);
                const float penetration = ball.radius - distance;
                if (penetration <= 0.0f)
                    continue;
                const vec3 away = distance >= minLength ? offset / distance : vec3{1.0f, 0.0f, 0.0f};
                const vec3 vertexVelocity = vec3{particleCrystal->particles[vertexIndex].velocity()};
                pushContact(state, candidate, Endpoint{Endpoint::Type::crystal, particleSide.body, static_cast<integer>(vertexIndex)}, Endpoint{Endpoint::Type::ball, ball.body, 0}, center + away * ball.radius, -away, penetration, vertexVelocity, ballVelocity);
            }
        }

        void separate(Contact& contact, float remaining, fqsm::Direct<Body> bodies, fqsm::Direct<Crystal> crystals) {
            auto* left = bodies.items.find(contact.a.body);
            auto* right = bodies.items.find(contact.b.body);
            if (not left or not right)
                return;
            const vec3 along = contact.normal;
            const float step = remaining;
            if (contact.a.type == Endpoint::Type::ball and contact.b.type == Endpoint::Type::ball) {
                const float weightA = inverseMass(left->mass);
                const float weightB = inverseMass(right->mass);
                const float weightSum = weightA + weightB;
                if (weightSum <= 0.0f)
                    return;
                left->position -= dvec3{along * (step * (weightA / weightSum))};
                right->position += dvec3{along * (step * (weightB / weightSum))};
                return;
            }
            if (contact.a.type == Endpoint::Type::ball) {
                left->position -= dvec3{along * step};
                return;
            }
            auto* particleCrystal = crystals.items.find(contact.a.body);
            if (not particleCrystal)
                return;
            kickVertex(*particleCrystal, contact.a.face, -along * step);
        }

        void frictionBallCrystal(Contact& contact, float normalStep, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            auto* body = bodies.items.find(contact.a.body);
            auto* ball = balls.items.find(contact.a.body);
            if (not body or not ball)
                return;
            const vec3 arm = contact.point - vec3{body->position};
            const vec3 slideVec = velocityAt(*body, *ball, contact.point) - velocityOf(contact.b.body, contact.b.type, bodies, balls, crystals);
            const vec3 tangentVel = slideVec - contact.normal * glm::dot(slideVec, contact.normal);
            const float slide = glm::length(tangentVel);
            if (slide < minLength)
                return;
            const vec3 tangent = tangentVel / slide;
            const float weight = spinWeight(*body, arm, tangent);
            if (weight <= 0.0f)
                return;
            float impulse = slide * Particle::dt / weight;
            const float limit = ballFriction * body->mass * normalStep;
            if (impulse > limit)
                impulse = limit;
            kickBall(*body, arm, -tangent * impulse);
        }

        void frictionBallBall(Contact& contact, float normalStep, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls) {
            auto* bodyA = bodies.items.find(contact.a.body);
            auto* bodyB = bodies.items.find(contact.b.body);
            auto* ballA = balls.items.find(contact.a.body);
            auto* ballB = balls.items.find(contact.b.body);
            if (not bodyA or not bodyB or not ballA or not ballB)
                return;
            const vec3 armA = contact.point - vec3{bodyA->position};
            const vec3 armB = contact.point - vec3{bodyB->position};
            const vec3 slideVec = velocityAt(*bodyA, *ballA, contact.point) - velocityAt(*bodyB, *ballB, contact.point);
            const vec3 tangentVel = slideVec - contact.normal * glm::dot(slideVec, contact.normal);
            const float slide = glm::length(tangentVel);
            if (slide < minLength)
                return;
            const vec3 tangent = tangentVel / slide;
            const float weight = spinWeight(*bodyA, armA, tangent) + spinWeight(*bodyB, armB, tangent);
            if (weight <= 0.0f)
                return;
            const float invSum = inverseMass(bodyA->mass) + inverseMass(bodyB->mass);
            if (invSum <= 0.0f)
                return;
            float impulse = slide * Particle::dt / weight;
            const float limit = ballFriction * (normalStep / invSum);
            if (impulse > limit)
                impulse = limit;
            kickBall(*bodyA, armA, -tangent * impulse);
            kickBall(*bodyB, armB, tangent * impulse);
        }

        void spinBalls(Contact& contact, float normalStep, fqsm::Direct<Body> bodies, fqsm::Direct<Ball> balls, fqsm::Direct<Crystal> crystals) {
            if (contact.a.type == Endpoint::Type::ball and contact.b.type == Endpoint::Type::ball)
                frictionBallBall(contact, normalStep, bodies, balls);
            else if (contact.a.type == Endpoint::Type::ball)
                frictionBallCrystal(contact, normalStep, bodies, balls, crystals);
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
                radius = glm::max(radius, glm::length(occupant.center - center) + occupant.radius);
            return true;
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
            else if (first.type == Endpoint::Type::ball and second.type == Endpoint::Type::crystal) {
                contactBallVsShape(state, candidate, first, second, bodies, balls, crystals);
                contactParticlesVsBall(state, candidate, second, first, bodies, balls, crystals);
            } else if (first.type == Endpoint::Type::crystal and second.type == Endpoint::Type::ball) {
                contactBallVsShape(state, candidate, second, first, bodies, balls, crystals);
                contactParticlesVsBall(state, candidate, first, second, bodies, balls, crystals);
            } else {
                contactParticlesVsShape(state, candidate, first, second, bodies, balls, crystals);
                contactParticlesVsShape(state, candidate, second, first, bodies, balls, crystals);
            }
        }

    }

    void State::build(Stewarding context) {
        candidates.clear();
        contacts.clear();
        auto bodies = context.direct<Body>();
        auto balls = context.direct<Ball>();
        auto crystals = context.direct<Crystal>();
        auto compounds = context.direct<Compound>();
        for (auto [id, crystal] : crystals.items)
            ensureBvh(crystal);
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
        auto balls = context.direct<Ball>();
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
            separate(contact, contact.penetration, bodies, crystals);
            spinBalls(contact, contact.penetration, bodies, balls, crystals);
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

}
