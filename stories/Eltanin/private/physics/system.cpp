#include "physics/system.h"
#include "physics/horn.h"

#include <base/logging.h>

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eltanin::phys {

    void System::applyForces(fqsm::Direct<Particle> particles) {
        // Central μ at origin (softened) — parked while the pendulum uses linear g.
        // for (auto [_, particle] : particles.items) {
        //     const vec3& sample = glm::dot(particle.prev, particle.prev) > glm::dot(particle.current, particle.current) ? particle.prev : particle.current;
        //     const float r2 = std::max(glm::dot(sample, sample), Settings::gravitySoften2);
        //     const float inv_r3 = 1.0f / (r2 * std::sqrt(r2));
        //     accelerations[slot++] = -Settings::centralMu * sample * inv_r3;
        // }
        accelerations.assign(particles.items.size(), vec3{0.0f, -Settings::gravity, 0.0f});
    }

    void System::integrate(fqsm::Direct<Particle> particles) {
        const float dt2 = Settings::fixedDtS * Settings::fixedDtS;
        std::size_t slot = 0;
        for (auto [_, particle] : particles.items) {
            const vec3 previous = particle.current;
            particle.current += particle.current - particle.prev + accelerations[slot++] * dt2;
            particle.prev = previous;
        }
    }

    void System::restoreBases(Stewarding context) {
        auto particles = context.direct<Particle>();
        auto atomics = context.direct<Atomic>();
        for (auto [_, atomic] : atomics.items) {
            const auto& rest = atomic.rest;
            const auto count = atomic.particles.size();
            if (count == 0 or rest.centered.size() != count) {
                continue;
            }

            vec3 com{0.0f, 0.0f, 0.0f};
            float mass_sum = 0.0f;
            bool missing = false;
            scratchWorldCentered.resize(count);
            scratchMasses.resize(count);
            for (std::size_t i = 0; i < count; ++i) {
                auto* particle = particles.items.find(atomic.particles[i]);
                if (not particle) {
                    missing = true;
                    break;
                }
                scratchWorldCentered[i] = particle->current;
                scratchMasses[i] = particle->mass;
                com += particle->current * particle->mass;
                mass_sum += particle->mass;
            }
            if (missing or mass_sum <= 0.0f) {
                continue;
            }
            com /= mass_sum;
            for (std::size_t i = 0; i < count; ++i) {
                scratchWorldCentered[i] -= com;
            }

            const quat rotation = horn::orientation(rest.centered, scratchWorldCentered, scratchMasses);

            for (std::size_t i = 0; i < count; ++i) {
                const vec3 goal = com + rotation * rest.centered[i];
                auto& particle = particles.items.at(atomic.particles[i]);
                // prev untouched: Verlet velocity shifts with current.
                particle.current += Settings::constraintStiffness * (goal - particle.current);
            }

            // Origin of pose: com − R·rest.com.
            atomic.restored = rmmr::Pose{
                .position = com - rotation * rest.com,
                .rotation = rotation,
            };
        }
    }

    void System::applyNails(Stewarding context) {
        auto particles = context.direct<Particle>();
        auto nails = context.direct<strong::Nail>();
        for (auto [id, nail] : nails.items) {
            auto* particle = particles.items.find(nail.particle);
            if (not particle) {
                with<strong::Nail>::remove(context, id);
                continue;
            }
            // prev untouched: Verlet velocity shifts with current (same as Horn).
            particle->current += Settings::constraintStiffness * (nail.point - particle->current);
        }
    }

    void System::applyGluons(Stewarding context) {
        auto particles = context.direct<Particle>();
        auto gluons = context.direct<strong::Gluon>();
        for (auto [id, gluon] : gluons.items) {
            std::erase_if(gluon.particles, [&](const Affected<Particle>& particle_id) {
                return particles.items.find(particle_id) == nullptr;
            });
            if (gluon.particles.size() < 2) {
                with<strong::Gluon>::remove(context, id);
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
                with<strong::Gluon>::remove(context, id);
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
        // Jakobsen: AccumulateForces → Verlet → constraint wave × N; Nail/Gluon seppuku via Writing under Stewarding.
        applyForces(context.direct<Particle>());
        integrate(context.direct<Particle>());
        for (int pass = 0; pass < Settings::constraintPasses; ++pass) {
            restoreBases(context);
            applyNails(context);
            applyGluons(context);
        }
    }

    void System::step(establish::Realm& world, int64 dt_us) {
        debt_us += static_cast<int64>(static_cast<double>(dt_us) * static_cast<double>(state.time_scale));
        if (debt_us < Settings::fixedStepUs) {
            return;
        }
        // One Stewarding for the whole catch-up loop (commit after last tick).
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
