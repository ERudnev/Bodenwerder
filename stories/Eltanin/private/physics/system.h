#pragma once

#include <eltanin/physics/rigid.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    // Private physics subsystem (not Q1).
    // Fixed tick 10ms; wall dt accumulates as debt (sub-step remainder).
    // One pass: accumulate forces (aerodynamics, gravity, …) → Verlet with dissipation → restore bases → connectivity → apply constraint wishes.
    // One Dock per tick; hot mutation via Stewarding::direct<rigid::Crystal>().
    // Orientation: Horn unit-quaternion method (symmetric N 4×4 + Jacobi), see physics/horn.h.
    // Thermal: small rocks only (no Volume). Accumulate to thermalStepUs, then radiate. No conduction.
    struct Settings {
        static constexpr float constraintStiffness = 1.0f;//0.75f; // Hitman-style goal pull (constraints)
        static constexpr float isaAirDensity = 1225.0f; // g/m³ ISA
        static constexpr float airDragTau = 1.0f; // seconds to e-fold at isaAirDensity
        static constexpr float dissipation = 0.99f; // Verlet (x−prev) scale per tick; 1 = none (Jakobsen ~0.99 for drag)
        static constexpr float restLinear = 1.0e-3f; // m/tick; below this (x−prev) is zeroed (0.99 never reaches 0)
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
        void accumulateForces(Stewarding);
        void applyAerodynamics(Stewarding);
        void integrate(fqsm::Direct<rigid::Crystal>);
        void integrateBalls(fqsm::Direct<rigid::Ball>);
        void restoreBases(Stewarding);
        void applyConnectivity(Stewarding);
        void applyConstraintWishes(Stewarding);
        void radiate(Stewarding);
    };

}
