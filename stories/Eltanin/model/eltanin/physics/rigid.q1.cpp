#include <eltanin/physics/rigid.q1.h>

#include "physics/horn.h"
#include "physics/system.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>

#include <cmath>

namespace eltanin::phys::rigid {

    using namespace rmmr;

    namespace {

        constexpr float axisEpsilon2 = 1.0e-12f;
        constexpr std::size_t octaCount = 6;

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

        auto currentComOf(const vector<Particle>& particles) -> vec3 {
            if (particles.empty())
                return vec3{0.0f, 0.0f, 0.0f};
            const vec3 anchor = particles.front().position;
            glm::dvec3 moment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (const Particle& particle : particles) {
                moment += glm::dvec3{particle.position - anchor} * static_cast<double>(particle.mass);
                mass += static_cast<double>(particle.mass);
            }
            return mass > 0.0 ? anchor + vec3{moment / mass} : vec3{0.0f, 0.0f, 0.0f};
        }

        auto rejectAlong(vec3 candidate, vec3 axis) -> vec3 {
            return candidate - axis * glm::dot(candidate, axis);
        }

        // goal = pose * shape; position = (1−k)·current + k·goal. Pose only; prev is impulse.
        void pullToShape(Crystal::Quantum& crystal, Pose pose) {
            const float k = Settings::constraintStiffness;
            for (std::size_t index = 0; index < crystal.particles.size(); ++index) {
                Particle& particle = crystal.particles[index];
                const vec3 goal = pose.position + pose.rotation * crystal.shape[index];
                particle.position = particle.position * (1.0f - k) + goal * k;
            }
            crystal.restored.pose(pose);
        }

    }

    auto restoredBody(Pose pose, const vector<Particle>& particles, const vector<vec3>& shape) -> Body {
        float mass = 0.0f;
        float thermalEnergy = 0.0f;
        float hitpoints = 0.0f;
        for (const Particle& particle : particles) {
            mass += particle.mass;
            thermalEnergy += particle.thermalEnergy();
            hitpoints += particle.hp();
        }
        const float temperature = mass > 0.0f ? thermalEnergy / mass : 0.0f;
        const float cohesion = mass > 0.0f ? hitpoints / mass : 0.0f;
        return Body{Matter{.position = pose.position, .mass = mass, .temperature = temperature, .cohesion = cohesion}, pose.rotation, radiusOf(particles, shape), hitpoints};
    }

    void Crystal::Quantum::refreshMatter() {
        restored = restoredBody(restored.pose(), particles, shape);
    }

    void Crystal::Actions::debugAddImpulse(Writing context, Id id, vec3 impulse) {
        auto crystal = with<Crystal>::modify(context, id);
        if (crystal->particles.empty())
            return;
        const vec3 share = impulse / static_cast<float>(crystal->particles.size());
        for (Particle& particle : crystal->particles) {
            if (particle.mass > 0.0f)
                particle.prev -= (share / particle.mass) * Particle::dt;
        }
    }

    void Crystal::Actions::setMotion(Writing context, Id id, Pose pose, vec3 linear, vec3 omega) {
        auto crystal = with<Crystal>::modify(context, id);
        if (crystal->particles.empty() or crystal->particles.size() != crystal->shape.size())
            return;
        const vec3 currentCom = pose.position + pose.rotation * crystal->com;
        for (std::size_t index = 0; index < crystal->particles.size(); ++index) {
            Particle& particle = crystal->particles[index];
            particle.position = pose.position + pose.rotation * crystal->shape[index];
            const vec3 spin = glm::cross(omega, particle.position - currentCom);
            particle.prev = particle.position - (linear + spin) * Particle::dt;
        }
        crystal->restored = restoredBody(pose, crystal->particles, crystal->shape);
    }

    void Octa::Actions::satisfy(Stewarding context) {
        auto crystals = context.direct<Crystal>();
        for (auto [id, _] : context.direct<Octa>().items) {
            auto* crystal = crystals.items.find(id);
            if (not crystal or crystal->particles.size() != octaCount or crystal->shape.size() != octaCount)
                continue;

            const vec3 currentCom = currentComOf(crystal->particles);
            vec3 axisX = crystal->particles[0].position - crystal->particles[1].position;
            const float lengthX2 = glm::dot(axisX, axisX);
            if (lengthX2 <= axisEpsilon2)
                continue;
            axisX /= std::sqrt(lengthX2);

            vec3 axisY = rejectAlong(crystal->particles[2].position - crystal->particles[3].position, axisX);
            if (glm::dot(axisY, axisY) <= axisEpsilon2)
                axisY = rejectAlong(crystal->particles[4].position - crystal->particles[5].position, axisX);
            const float lengthY2 = glm::dot(axisY, axisY);
            if (lengthY2 <= axisEpsilon2)
                continue;
            axisY /= std::sqrt(lengthY2);

            const vec3 axisZ = glm::cross(axisX, axisY);
            quat rotation = glm::quat_cast(mat3{axisX, axisY, axisZ});
            if (glm::dot(rotation, crystal->restored.orientation) < 0.0f)
                rotation = -rotation;
            pullToShape(*crystal, Pose{.position = currentCom - rotation * crystal->com, .rotation = rotation});
        }
    }

    void Horned::Actions::satisfy(Stewarding context) {
        auto crystals = context.direct<Crystal>();
        vector<vec3> restCentered;
        vector<vec3> worldCentered;
        vector<float> masses;
        for (auto [id, _] : context.direct<Horned>().items) {
            auto* crystal = crystals.items.find(id);
            if (not crystal)
                continue;
            const std::size_t count = crystal->particles.size();
            if (count == 0 or crystal->shape.size() != count)
                continue;

            const vec3 currentCom = currentComOf(crystal->particles);
            const vec3 restCom = crystal->com;
            restCentered.resize(count);
            worldCentered.resize(count);
            masses.resize(count);
            float mass = 0.0f;
            for (std::size_t index = 0; index < count; ++index) {
                restCentered[index] = crystal->shape[index] - restCom;
                worldCentered[index] = crystal->particles[index].position - currentCom;
                masses[index] = crystal->particles[index].mass;
                mass += masses[index];
            }
            if (mass <= 0.0f)
                continue;

            const quat rotation = horn::orientation(restCentered, worldCentered, masses);
            pullToShape(*crystal, Pose{.position = currentCom - rotation * restCom, .rotation = rotation});
        }
    }

    auto CelestialGravity::Quantum::roundOrbitHelper(float distance) const -> float {
        if (distance < averageRadius or averageRadius <= 0.0f or surfaceAcceleration <= 0.0f)
            return 0.0f;
        return std::sqrt(surfaceAcceleration * averageRadius * averageRadius / distance);
    }

    // Verlet: position += a·dt² toward the source COM. Outside ~ 1/r²; inside a uniform sphere, linear in r.
    void CelestialGravity::Actions::apply(Stewarding context) {
        auto crystals = context.direct<Crystal>();
        const float dt2 = Particle::dt * Particle::dt;
        for (auto [sourceId, gravity] : context.direct<CelestialGravity>().items) {
            if (gravity.averageRadius <= 0.0f or gravity.surfaceAcceleration == 0.0f)
                continue;
            auto* source = crystals.items.find(sourceId);
            if (not source or source->particles.empty())
                continue;
            const vec3 sourceCom = currentComOf(source->particles);
            const float radius = gravity.averageRadius;
            const float surface = gravity.surfaceAcceleration;
            for (auto [targetId, crystal] : crystals.items) {
                if (targetId == sourceId)
                    continue;
                for (Particle& particle : crystal.particles) {
                    const vec3 offset = particle.position - sourceCom;
                    const float distance2 = glm::dot(offset, offset);
                    if (distance2 <= 1.0e-12f)
                        continue;
                    const float distance = std::sqrt(distance2);
                    const float accelScale = distance < radius ? -surface / radius : -surface * radius * radius / (distance2 * distance);
                    particle.position += offset * accelScale * dt2;
                }
            }
        }
    }

}
