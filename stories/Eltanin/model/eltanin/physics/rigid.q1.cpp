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

        // goal[i] = currentCOM + R·q[i], q = shape − restCOM. Pulls position only; prev is impulse.
        // Subtracts mass-weighted mean correction so the shape solver cannot translate COM.
        void pullToShape(Crystal::Quantum& crystal, quat rotation, vec3 currentCom) {
            const vec3 restCom = crystal.com;
            glm::dvec3 correctionMoment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (std::size_t index = 0; index < crystal.particles.size(); ++index) {
                const vec3 q = crystal.shape[index] - restCom;
                const vec3 goal = currentCom + rotation * q;
                const vec3 correction = goal - crystal.particles[index].position;
                correctionMoment += static_cast<double>(crystal.particles[index].mass) * glm::dvec3{correction};
                mass += static_cast<double>(crystal.particles[index].mass);
            }
            const vec3 meanCorrection = mass > 0.0 ? vec3{correctionMoment / mass} : vec3{0.0f, 0.0f, 0.0f};
            for (std::size_t index = 0; index < crystal.particles.size(); ++index) {
                Particle& particle = crystal.particles[index];
                const vec3 q = crystal.shape[index] - restCom;
                const vec3 goal = currentCom + rotation * q;
                particle.position += Settings::constraintStiffness * (goal - particle.position - meanCorrection);
            }
            crystal.restored = restoredBody(Pose{.position = currentCom - rotation * restCom, .rotation = rotation}, crystal.particles, crystal.shape);
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

    void Crystal::Actions::setMotion(Writing context, Id id, vec3 linear, vec3 omega) {
        auto crystal = with<Crystal>::modify(context, id);
        if (crystal->particles.empty())
            return;
        const vec3 currentCom = currentComOf(crystal->particles);
        for (Particle& particle : crystal->particles) {
            const vec3 spin = glm::cross(omega, particle.position - currentCom);
            particle.prev = particle.position - (linear + spin) * Particle::dt;
        }
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
            pullToShape(*crystal, rotation, currentCom);
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
            pullToShape(*crystal, rotation, currentCom);
        }
    }

}
