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
        Atomic::Actions::satisfy(context);
        Clast::Actions::satisfy(context);
        strong::Nail::Actions::satisfy(context);
        strong::Gluon::Actions::satisfy(context);
    }

    void System::tick(Stewarding context) {
        // Jakobsen: Verlet → constraint wave × N; Nail/Gluon seppuku via Writing under Stewarding.
        integrate(context.direct<Particle>());
        for (int pass = 0; pass < Settings::constraintPasses; ++pass) {
            constraintPass(context);
        }
        ::eltanin::geo::Boulder::Actions::syncPose(context);
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
