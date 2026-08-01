#include "physics/system.h"
#include "physics/horn.h"

#include <eltanin/resources/atomic.q1.h>

#include <base/logging.h>

#include <algorithm>
#include <cmath>
#include <format>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eltanin::phys {

    void System::applyForces(fqsm::Direct<Particle>& particles) {
        // Central μ at origin (softened) — parked while the pendulum uses linear g.
        // for (auto [_, particle] : particles.items) {
        //     const vec3& sample = glm::dot(particle.prev, particle.prev) > glm::dot(particle.current, particle.current) ? particle.prev : particle.current;
        //     const float r2 = std::max(glm::dot(sample, sample), Settings::gravitySoften2);
        //     const float inv_r3 = 1.0f / (r2 * std::sqrt(r2));
        //     accelerations[slot++] = -Settings::centralMu * sample * inv_r3;
        // }
        accelerations.assign(particles.items.size(), vec3{0.0f, -Settings::gravity, 0.0f});
    }

    void System::integrate(fqsm::Direct<Particle>& particles) {
        const float dt2 = Settings::fixedDtS * Settings::fixedDtS;
        std::size_t slot = 0;
        for (auto [_, particle] : particles.items) {
            const vec3 previous = particle.current;
            particle.current += particle.current - particle.prev + accelerations[slot++] * dt2;
            particle.prev = previous;
        }
    }

    void System::restoreBases(Stewarding context, fqsm::Direct<Particle>& particles, fqsm::Direct<Atomic>& atomics) {
        for (auto [_, atomic] : atomics.items) {
            const auto& shape = with<resource::atomic::Asset>::get(context, atomic.shape);
            const auto count = atomic.particles.size();
            if (count == 0) {
                continue;
            }
            if (shape.points.size() != count) {
                (void)context.refuse(std::format(
                    "eltanin::phys::System::restoreBases: Atomic particle/shape size mismatch ({} vs {})",
                    count,
                    shape.points.size()));
                continue;
            }

            vector<vec3> world;
            vector<vec3> rest;
            vector<float> masses;
            world.reserve(count);
            rest.reserve(count);
            masses.reserve(count);

            vec3 com{0.0f, 0.0f, 0.0f};
            vec3 rest_com{0.0f, 0.0f, 0.0f};
            float mass_sum = 0.0f;
            bool missing = false;
            for (std::size_t i = 0; i < count; ++i) {
                auto* particle = particles.items.find(atomic.particles[i]);
                if (not particle) {
                    missing = true;
                    break;
                }
                world.push_back(particle->current);
                rest.push_back(shape.points[i]);
                masses.push_back(particle->mass);
                com += particle->current * particle->mass;
                rest_com += shape.points[i] * particle->mass;
                mass_sum += particle->mass;
            }
            if (missing) {
                (void)context.refuse("eltanin::phys::System::restoreBases: Atomic references missing Particle");
                continue;
            }
            if (mass_sum <= 0.0f) {
                (void)context.refuse("eltanin::phys::System::restoreBases: Atomic total mass is non-positive");
                continue;
            }
            com /= mass_sum;
            rest_com /= mass_sum;

            vector<vec3> world_centered;
            vector<vec3> rest_centered;
            world_centered.reserve(count);
            rest_centered.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                world_centered.push_back(world[i] - com);
                rest_centered.push_back(rest[i] - rest_com);
            }

            const quat rotation = horn::orientation(rest_centered, world_centered, masses);

            for (std::size_t i = 0; i < count; ++i) {
                const vec3 goal = com + rotation * rest_centered[i];
                auto& particle = particles.items.at(atomic.particles[i]);
                // prev untouched: Verlet velocity shifts with current.
                particle.current += Settings::constraintStiffness * (goal - particle.current);
            }

            // Mesh verts = shape.points (game meters). Origin of pose must match Horn centering: com − R·rest_com.
            atomic.restored = rmmr::Pose{
                .position = com - rotation * rest_com,
                .rotation = rotation,
            };
        }
    }

    void System::applyNails(fqsm::Direct<Particle>& particles, fqsm::Direct<strong::Nail>& nails, vector<strong::Nail::Id>& doomed) {
        for (auto [id, nail] : nails.items) {
            auto* particle = particles.items.find(nail.particle);
            if (not particle) {
                doomed.push_back(id);
                continue;
            }
            // prev untouched: Verlet velocity shifts with current (same as Horn).
            particle->current += Settings::constraintStiffness * (nail.point - particle->current);
        }
    }

    void System::applyGluons(fqsm::Direct<Particle>& particles, fqsm::Direct<strong::Gluon>& gluons, vector<strong::Gluon::Id>& doomed) {
        for (auto [id, gluon] : gluons.items) {
            std::erase_if(gluon.particles, [&](const Affected<Particle>& particle_id) {
                return particles.items.find(particle_id) == nullptr;
            });
            if (gluon.particles.size() < 2) {
                doomed.push_back(id);
                continue;
            }

            vec3 com{0.0f, 0.0f, 0.0f};
            float mass_sum = 0.0f;
            for (const auto particle_id : gluon.particles) {
                const auto& particle = particles.items.at(particle_id);
                com += particle.current * particle.mass;
                mass_sum += particle.mass;
            }
            if (mass_sum <= 0.0f) {
                doomed.push_back(id);
                continue;
            }
            com /= mass_sum;
            for (const auto particle_id : gluon.particles) {
                auto& particle = particles.items.at(particle_id);
                particle.current += Settings::constraintStiffness * (com - particle.current);
            }
        }
    }

    void System::tick(Stewarding context) {
        // Jakobsen: AccumulateForces → Verlet → constraint wave × N — Direct; seppuku after Breach closes.
        vector<strong::Nail::Id> deadNails;
        vector<strong::Gluon::Id> deadGluons;
        {
            fqsm::Direct<Particle> particles = context;
            fqsm::Direct<Atomic> atomics = context;
            fqsm::Direct<strong::Nail> nails = context;
            fqsm::Direct<strong::Gluon> gluons = context;
            applyForces(particles);
            integrate(particles);
            for (int pass = 0; pass < Settings::constraintPasses; ++pass) {
                deadNails.clear();
                deadGluons.clear();
                restoreBases(context, particles, atomics);
                applyNails(particles, nails, deadNails);
                applyGluons(particles, gluons, deadGluons);
            }
        }
        Writing writing = context;
        for (const auto id : deadNails) {
            with<strong::Nail>::remove(writing, id);
        }
        for (const auto id : deadGluons) {
            with<strong::Gluon>::remove(writing, id);
        }
    }

    void System::step(establish::Realm& world, int64 dt_us) {
        debt_us += static_cast<int64>(static_cast<double>(dt_us) * static_cast<double>(state.time_scale));
        if (debt_us < Settings::fixedStepUs) {
            return;
        }
        Stewarding session = world;
        while (debt_us >= Settings::fixedStepUs) {
            tick(session);
            debt_us -= Settings::fixedStepUs;
        }
    }

    auto System::addParticle(Writing context, vec3 pos, vec3 velocity, float mass) -> Particle::Id {
        // Verlet: v ≈ (current − prev) / dt  ⇒  prev = current − v·dt
        return with<Particle>::create(context, Particle::Quantum{
            .current = pos,
            .prev = pos - velocity * Settings::fixedDtS,
            .mass = mass,
        });
    }

}
