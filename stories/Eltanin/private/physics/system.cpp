#include "physics/system.h"
#include "physics/horn.h"

#include <eltanin/resources/atomic.q1.h>

#include <base/logging.h>

#include <cmath>
#include <format>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eltanin::phys {

    namespace {

        constexpr int64 k_fixed_step_us = 10'000; // 10 ms
        // Softened magic point-mass at origin.
        constexpr float k_gravity_soften = 0.25f;
        constexpr float k_soften2 = k_gravity_soften * k_gravity_soften;

    } // namespace

    void System::applyForces(fqsm::Direct<Particle>& particles) {
        accelerations.resize(particles.items.size());
        std::size_t slot = 0;
        for (auto [_, particle] : particles.items) {
            // a = −μ r / r³ (softened), independent of particle mass.
            const float r2 = std::max(glm::dot(particle.current, particle.current), k_soften2);
            const float inv_r3 = 1.0f / (r2 * std::sqrt(r2));
            accelerations[slot++] = -k_central_mu * particle.current * inv_r3;
        }
    }

    void System::integrate(fqsm::Direct<Particle>& particles) {
        const float dt2 = k_fixed_dt_s * k_fixed_dt_s;
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
                particle.current += k_constraint_stiffness * (goal - particle.current);
            }

            // Mesh verts = shape.points. Origin of pose must match Horn centering: com − R·rest_com.
            atomic.restored = rmmr::Pose{
                .position = com - rotation * rest_com,
                .rotation = rotation,
            };
        }
    }

    void System::tick(Stewarding context) {
        // Jakobsen: AccumulateForces → Verlet → Horn rigid — all Direct, no Writing.
        fqsm::Direct<Particle> particles = context;
        fqsm::Direct<Atomic> atomics = context;
        applyForces(particles);
        integrate(particles);
        restoreBases(context, particles, atomics);
    }

    void System::step(establish::Realm& world, int64 dt_us) {
        debt_us += static_cast<int64>(static_cast<double>(dt_us) * static_cast<double>(state.time_scale));
        if (debt_us < k_fixed_step_us) {
            return;
        }
        Stewarding session = world;
        while (debt_us >= k_fixed_step_us) {
            tick(session);
            debt_us -= k_fixed_step_us;
        }
    }

    auto System::addParticle(Writing context, vec3 pos, vec3 velocity, float mass) -> Particle::Id {
        // Verlet: v ≈ (current − prev) / dt  ⇒  prev = current − v·dt
        return with<Particle>::create(context, Particle::Quantum{
            .current = pos,
            .prev = pos - velocity * k_fixed_dt_s,
            .mass = mass,
        });
    }

}
