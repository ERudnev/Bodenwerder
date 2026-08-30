#include <eltanin/physics/rigid.q1.h>

#include "physics/horn.h"
#include "physics/system.h"

#include <glm/geometric.hpp>

#include <cmath>

namespace eltanin::phys::rigid {

    using namespace rmmr;

    namespace {

        auto restComOf(const vector<Particle>& particles, const vector<vec3>& shape) -> vec3 {
            glm::dvec3 moment{0.0, 0.0, 0.0};
            double mass = 0.0;
            const std::size_t count = particles.size() < shape.size() ? particles.size() : shape.size();
            for (std::size_t index = 0; index < count; ++index) {
                moment += glm::dvec3{shape[index]} * static_cast<double>(particles[index].mass);
                mass += static_cast<double>(particles[index].mass);
            }
            return mass > 0.0 ? vec3{moment / mass} : vec3{0.0f, 0.0f, 0.0f};
        }

        auto radiusOf(const vector<Particle>& particles, const vector<vec3>& shape) -> float {
            const vec3 restCom = restComOf(particles, shape);
            float radius = 0.0f;
            const std::size_t count = particles.size() < shape.size() ? particles.size() : shape.size();
            for (std::size_t index = 0; index < count; ++index)
                radius = glm::max(radius, glm::length(shape[index] - restCom));
            return radius;
        }

        auto currentComOf(const vector<Particle>& particles) -> dvec3 {
            dvec3 moment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (const Particle& particle : particles) {
                moment += particle.position * double(particle.mass);
                mass += double(particle.mass);
            }
            return mass > 0.0 ? moment / mass : dvec3{0.0, 0.0, 0.0};
        }

        // goal = origin + R·shape; position += k·(goal−position). Same delta on prev so projection does not invent Verlet velocity.
        void pullToShape(Crystal::Quantum& crystal, dvec3 origin, quat rotation) {
            const double k = double(Settings::constraintStiffness);
            for (std::size_t index = 0; index < crystal.particles.size(); ++index) {
                Particle& particle = crystal.particles[index];
                const dvec3 goal = origin + dvec3{rotation * crystal.shape[index]};
                const dvec3 correction = (goal - particle.position) * k;
                particle.position += correction;
                particle.prev += correction;
            }
        }

    }

    auto restoredBody(dvec3 origin, quat rotation, const vector<Particle>& particles, const vector<vec3>& shape) -> Body::Quantum {
        float mass = 0.0f;
        for (const Particle& particle : particles)
            mass += particle.mass;
        return Body::Quantum{.position = origin, .orientation = rotation, .totalMass = mass, .radius = radiusOf(particles, shape)};
    }

    auto restoredBody(Pose pose, const vector<Particle>& particles, const vector<vec3>& shape) -> Body::Quantum {
        return restoredBody(dvec3{pose.position}, pose.rotation, particles, shape);
    }

    void Crystal::Quantum::refreshMatter(Body::Quantum& body) {
        body = restoredBody(body.position, body.orientation, particles, shape);
    }

    void Crystal::Actions::debugAddImpulse(Writing context, Id id, vec3 impulse) {
        auto crystal = with<Crystal>::modify(context, id);
        if (crystal->particles.empty())
            return;
        const vec3 share = impulse / static_cast<float>(crystal->particles.size());
        for (Particle& particle : crystal->particles) {
            if (particle.mass > 0.0f)
                particle.force += share / float(Settings::fixedStep);
        }
    }

    void Crystal::Actions::setMotion(Writing context, Id id, Pose pose, vec3 linear, vec3 omega) {
        auto crystal = with<Crystal>::modify(context, id);
        auto body = with<Body>::modify(context, id);
        if (crystal->particles.empty() or crystal->particles.size() != crystal->shape.size())
            return;
        const dvec3 origin{pose.position};
        const dvec3 currentCom = origin + dvec3{pose.rotation * crystal->com};
        for (std::size_t index = 0; index < crystal->particles.size(); ++index) {
            Particle& particle = crystal->particles[index];
            particle.position = origin + dvec3{pose.rotation * crystal->shape[index]};
            const vec3 spin = glm::cross(omega, vec3{particle.position - currentCom});
            particle.prev = particle.position - dvec3{(linear + spin) * float(Settings::fixedStep)};
            particle.force = vec3{0.0f, 0.0f, 0.0f};
        }
        *body = restoredBody(origin, pose.rotation, crystal->particles, crystal->shape);
    }

    void Crystal::Actions::restore(Stewarding context) {
        auto bodies = context.direct<Body>();
        vector<vec3> restCentered;
        vector<vec3> worldCentered;
        vector<float> masses;
        for (auto [id, crystal] : context.direct<Crystal>().items) {
            auto* body = bodies.items.find(id);
            if (not body)
                continue;
            const std::size_t count = crystal.particles.size();
            if (count == 0 or crystal.shape.size() != count)
                continue;

            const dvec3 currentCom = currentComOf(crystal.particles);
            const vec3 restCom = crystal.com;
            restCentered.resize(count);
            worldCentered.resize(count);
            masses.resize(count);
            float mass = 0.0f;
            for (std::size_t index = 0; index < count; ++index) {
                restCentered[index] = crystal.shape[index] - restCom;
                worldCentered[index] = vec3{crystal.particles[index].position - currentCom};
                masses[index] = crystal.particles[index].mass;
                mass += masses[index];
            }
            if (mass <= 0.0f)
                continue;

            const quat rotation = horn::orientation(restCentered, worldCentered, masses);
            body->orientation = rotation;
            body->position = currentCom - dvec3{rotation * restCom};
        }
    }

    void Crystal::Actions::applyRestored(Stewarding context) {
        auto bodies = context.direct<Body>();
        for (auto [id, crystal] : context.direct<Crystal>().items) {
            auto* body = bodies.items.find(id);
            if (not body)
                continue;
            if (crystal.particles.empty() or crystal.particles.size() != crystal.shape.size())
                continue;
            pullToShape(crystal, body->position, body->orientation);
        }
    }

    auto CelestialGravity::Quantum::roundOrbitHelper(float distance) const -> float {
        if (distance < averageRadius or averageRadius <= 0.0f or surfaceAcceleration <= 0.0f)
            return 0.0f;
        return std::sqrt(surfaceAcceleration * averageRadius * averageRadius / distance);
    }

    // Outside ~ 1/r²; inside a uniform sphere, linear in r.
    // Each pull on a victim gets −F on the source, mass-weighted onto source particles once per source per tick.
    void CelestialGravity::Actions::apply(Stewarding context) {
        auto crystals = context.direct<Crystal>();
        auto solids = context.direct<Solid>();
        auto bodies = context.direct<Body>();
        for (auto [sourceId, gravity] : context.direct<CelestialGravity>().items) {
            if (gravity.averageRadius <= 0.0f or gravity.surfaceAcceleration == 0.0f)
                continue;
            auto* source = crystals.items.find(sourceId);
            if (not source or source->particles.empty())
                continue;
            const dvec3 sourceCom = currentComOf(source->particles);
            const double radius = double(gravity.averageRadius);
            const double surface = double(gravity.surfaceAcceleration);
            dvec3 recoil{0.0, 0.0, 0.0};
            for (auto [targetId, crystal] : crystals.items) {
                if (targetId == sourceId)
                    continue;
                for (Particle& particle : crystal.particles) {
                    const dvec3 offset = particle.position - sourceCom;
                    const double distance2 = glm::dot(offset, offset);
                    if (distance2 <= 1.0e-12)
                        continue;
                    const double distance = std::sqrt(distance2);
                    const double accelScale = distance < radius ? -surface / radius : -surface * radius * radius / (distance2 * distance);
                    const dvec3 force = offset * (accelScale * double(particle.mass));
                    particle.force += vec3{force};
                    recoil -= force;
                }
            }
            for (auto [id, solid] : solids.items) {
                auto* body = bodies.items.find(id);
                if (not body or body->totalMass <= 0.0f)
                    continue;
                const dvec3 offset = body->position - sourceCom;
                const double distance2 = glm::dot(offset, offset);
                if (distance2 <= 1.0e-12)
                    continue;
                const double distance = std::sqrt(distance2);
                const double accelScale = distance < radius ? -surface / radius : -surface * radius * radius / (distance2 * distance);
                const dvec3 force = offset * (accelScale * double(body->totalMass));
                solid.center.force += vec3{force};
                recoil -= force;
            }
            for (auto [_, ray] : context.direct<Ray>().items) {
                if (ray.core.mass <= 0.0f)
                    continue;
                const dvec3 offset = ray.core.position - sourceCom;
                const double distance2 = glm::dot(offset, offset);
                if (distance2 <= 1.0e-12)
                    continue;
                const double distance = std::sqrt(distance2);
                const double accelScale = distance < radius ? -surface / radius : -surface * radius * radius / (distance2 * distance);
                const dvec3 force = offset * (accelScale * double(ray.core.mass));
                ray.core.force += vec3{force};
                recoil -= force;
            }
            double sourceMass = 0.0;
            for (const Particle& particle : source->particles)
                sourceMass += double(particle.mass);
            if (sourceMass <= 1.0e-12)
                continue;
            for (Particle& particle : source->particles) {
                if (particle.mass <= 0.0f)
                    continue;
                particle.force += vec3{recoil * (double(particle.mass) / sourceMass)};
            }
        }
    }

}
