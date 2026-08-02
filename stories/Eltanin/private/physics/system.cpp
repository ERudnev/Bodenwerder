#include "physics/system.h"

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

    void System::constraintPass(Stewarding context) {
        Atomic::Actions::satisfy(context);
        strong::Nail::Actions::satisfy(context);
        strong::Gluon::Actions::satisfy(context);
    }

    void System::tick(Stewarding context) {
        // Jakobsen: AccumulateForces → Verlet → constraint wave × N; Nail/Gluon seppuku via Writing under Stewarding.
        applyForces(context.direct<Particle>());
        integrate(context.direct<Particle>());
        for (int pass = 0; pass < Settings::constraintPasses; ++pass) {
            constraintPass(context);
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
