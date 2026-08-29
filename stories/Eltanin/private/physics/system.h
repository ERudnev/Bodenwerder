#pragma once

#include "physics/collisions.h"

#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/scene/root.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    // Private physics subsystem (not Q1).
    // Fixed tick 10ms; wall dt accumulates as debt (sub-step remainder).
    // One pass: accumulate forces → Verlet → Horn (query pose) → connectivity → Horn (absorb kicks) → pull to shape.
    // One Dock per tick; hot mutation via Stewarding::direct<Body>() and direct<rigid::Crystal>() / direct<rigid::Solid>().
    // Orientation: Horn unit-quaternion method (symmetric N 4×4 + Jacobi), see physics/horn.h.
    // Thermal: Boulder. Rock Volume thermal deferred. Accumulate to thermalStepUs, then radiate. No conduction.
    struct Settings {
        static constexpr float constraintStiffness = 1.0f;//0.75f; // Hitman-style goal pull (constraints)
        static constexpr float isaAirDensity = 1225.0f; // g/m³ ISA
        static constexpr float airDragTau = 1.0f; // seconds to e-fold linear speed at isaAirDensity
        static constexpr float airSpinHalfLife = 3.0f; // Solid ω halves in this many seconds at isaAirDensity
        static constexpr float restLinear = 1.0e-5f; // m/tick; below this (x−prev) is zeroed
        static constexpr float solidLiveSpeed = 0.1f; // m/s; Solid↔Crystal — soft fade of restitution and spin below this closing speed
        static constexpr float cohesionWound = 2.5f; // Δcohesion = cohesionWound · |p_ray| / m_face; 30mm 0.4 kg × 200 m/s vs 4 t plate → 5%
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
        scene::Root::Id scene;

        System(scene::Root::Id);
        void step(establish::Realm&, int64 dtUs);

    private:
        int64 debtUs;
        int64 thermalDebtUs;
        collision::State collisions;

        void tick(Stewarding);
        void accumulateForces(Stewarding);
        void applyAerodynamics(Stewarding);
        void applyLinearGravity(Stewarding);
        void integrate(fqsm::Direct<rigid::Crystal>);
        void integrateSolids(fqsm::Direct<Body>, fqsm::Direct<rigid::Solid>);
        void integrateRays(fqsm::Direct<Body>, fqsm::Direct<rigid::Ray>);
        void restoreBases(Stewarding);
        void applyConnectivity(Stewarding);
        void applyConstraintWishes(Stewarding);
        void radiate(Stewarding);
    };

}
