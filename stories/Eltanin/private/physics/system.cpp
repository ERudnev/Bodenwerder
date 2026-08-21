#include "physics/system.h"

#include <eltanin/geo/rock.q1.h>

namespace eltanin::phys {

    System::System()
        : state{.timeScale = 1.0f}
        , debtUs(0)
        , thermalDebtUs(0) {
    }

    void System::integrate(fqsm::Direct<rigid::Crystal> crystals) {
        for (auto [_, crystal] : crystals.items) {
            for (Particle& particle : crystal.particles) {
                const vec3 previous = particle.position;
                particle.position += particle.position - particle.prev;
                particle.prev = previous;
            }
        }
    }

    void System::constraintPass(Stewarding context) {
        with<rigid::Octa>::satisfy(context);
        with<rigid::Horned>::satisfy(context);
    }

    void System::tick(Stewarding context) {
        integrate(context.direct<rigid::Crystal>());
        with<rigid::CelestialGravity>::apply(context);
        for (int pass = 0; pass < Settings::constraintPasses; ++pass)
            constraintPass(context);
    }

    void System::radiate(Stewarding context) {
        if (thermalDebtUs < Settings::thermalStepUs)
            return;
        with<::eltanin::geo::Rock>::radiate(context, static_cast<float>(thermalDebtUs) * 1e-6f);
        thermalDebtUs = 0;
    }

    void System::step(establish::Realm& world, int64 dtUs) {
        const int64 scaled = static_cast<int64>(static_cast<double>(dtUs) * static_cast<double>(state.timeScale));
        debtUs += scaled;
        thermalDebtUs += scaled;
        if (debtUs < Settings::fixedStepUs and thermalDebtUs < Settings::thermalStepUs)
            return;
        Stewarding session = world;
        while (debtUs >= Settings::fixedStepUs) {
            tick(session);
            debtUs -= Settings::fixedStepUs;
        }
        radiate(session);
    }

}
