#pragma once

#include <eltanin/physics/rigid.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    // Private physics subsystem (not Q1).
    // Fixed tick 10ms; wall dt accumulates as debt (sub-step remainder).
    // Verlet + CelestialGravity (a·dt² on other crystals) + rigid constraints.
    // One Dock per tick; hot mutation via Stewarding::direct<rigid::Crystal>().
    // Orientation: Horn unit-quaternion method (symmetric N 4×4 + Jacobi), see physics/horn.h.
    // Constraint wave: each installed rigid feature contributes its Crystal solver.
    // Thermal: small rocks only (no Volume). Accumulate to thermalStepUs, then radiate. No conduction.
    struct Settings {
        static constexpr float constraintStiffness = 0.75f; // Hitman-style goal pull (constraints)
        static constexpr int constraintPasses = 4;
        static constexpr int64 fixedStepUs = 10'000;
        static constexpr int64 thermalStepUs = 200'000;
        static constexpr float skyKelvin = 3.0f;
        static constexpr float radiateSigma = 5.0e-13f; // parrot mass × kelvin; five times slower than the first lava-scale guess
    };
    static_assert(Particle::dt == static_cast<float>(Settings::fixedStepUs) / 1'000'000.0f);

    struct System {
        struct State {
            float timeScale; // wall dt → physics time
        };
        State state;

        System();
        void step(establish::Realm&, int64 dtUs);

    private:
        int64 debtUs;
        int64 thermalDebtUs;

        void tick(Stewarding);
        void radiate(Stewarding);
        void constraintPass(Stewarding);
        void integrate(fqsm::Direct<rigid::Crystal>);
    };

}
