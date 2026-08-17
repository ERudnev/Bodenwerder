#include "physics/system.h"

#include <eltanin/geo/boulder.q1.h>

#include <base/logging.h>

namespace eltanin::phys {

    void System::integrate(fqsm::Direct<Particle> particles) {
        for (auto [_, particle] : particles.items) {
            const vec3 previous = particle.current;
            particle.current += particle.current - particle.prev;
            particle.prev = previous;
        }
    }

    void System::constraintPass(Stewarding context) {
        with<Atomic>::satisfy(context);
        with<Clast>::satisfy(context);
        with<strong::Nail>::satisfy(context);
        with<strong::Gluon>::satisfy(context);
    }

    void System::tick(Stewarding context) {
        // Jakobsen: Verlet → constraint wave × N; Nail/Gluon seppuku via Writing under Stewarding.
        integrate(context.direct<Particle>());
        for (int pass = 0; pass < Settings::constraintPasses; ++pass) {
            constraintPass(context);
        }
        with<::eltanin::geo::Boulder>::syncPose(context);
    }

    void System::radiate(Stewarding context) {
        if (thermalDebtUs < Settings::thermalStepUs)
            return;
        with<::eltanin::geo::Boulder>::radiate(context, static_cast<float>(thermalDebtUs) * 1e-6f);
        thermalDebtUs = 0;
    }

    void System::step(establish::Realm& world, int64 dt_us) {
        const int64 scaled = static_cast<int64>(static_cast<double>(dt_us) * static_cast<double>(state.time_scale));
        debt_us += scaled;
        thermalDebtUs += scaled;
        if (debt_us < Settings::fixedStepUs and thermalDebtUs < Settings::thermalStepUs)
            return;
        Stewarding session = world;
        while (debt_us >= Settings::fixedStepUs) {
            tick(session);
            debt_us -= Settings::fixedStepUs;
        }
        radiate(session);
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
