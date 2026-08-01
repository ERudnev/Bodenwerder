#include "physics/system.h"

#include <base/logging.h>

#include <cmath>

#include <glm/geometric.hpp>

namespace eltanin::phys {

    namespace {

        constexpr int64 k_fixed_step_us = 10'000; // 10 ms
        constexpr float k_fixed_dt_s = 0.01f;

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

    void System::restoreBases(Stewarding) {}

    void System::tick(Stewarding context) {
        // Jakobsen TimeStep: AccumulateForces → Verlet → (Horn) → SatisfyConstraints (later).
        {
            fqsm::Direct<Particle> particles = context;
            applyForces(particles);
            integrate(particles);
        }
        restoreBases(context);
    }

    void System::step(Stewarding context, int64 dt_us) {
        debt_us += static_cast<int64>(static_cast<double>(dt_us) * static_cast<double>(state.time_scale));
        while (debt_us >= k_fixed_step_us) {
            tick(context);
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
