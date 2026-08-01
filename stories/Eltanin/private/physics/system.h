#pragma once

#include <eltanin/physics/atomic.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <eltanin/physics/strong.q1.h>

#include <fQSM/api/interface.h>

#include <vector>

namespace eltanin::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    // Private physics subsystem (not Q1).
    // Fixed tick 10ms; wall dt accumulates as debt (sub-step remainder).
    // Magic point-mass at origin: a = −μ r / r³ (softened). Spawn circular speed ≈ sqrt(μ/r).
    // Entire step is Direct (Particle + Atomic + strong::Nail) inside one Dock; presentation reacts after commit.
    // Orientation: Horn unit-quaternion method (symmetric N 4×4 + Jacobi), see physics/horn.h.
    // Constraint wave: Atomic Horn soft pull, then strong::Nail pins (same stiffness).
    struct Settings {
        static constexpr float centralMu = 8.0f;
        static constexpr float constraintStiffness = 0.75f; // Hitman-style goal pull (constraints)
        static constexpr float massMin = 1.0f;
        static constexpr float massMax = 100.0f;
        static constexpr int64 fixedStepUs = 10'000;
        static constexpr float fixedDtS = static_cast<float>(fixedStepUs) / 1'000'000.0f;
        static constexpr float gravitySoften = 0.25f;
        static constexpr float gravitySoften2 = gravitySoften * gravitySoften;
    };

    struct System {
        struct State {
            float time_scale = 1.0f; // wall dt → physics time
        };
        State state;

        void step(establish::Realm&, int64 dt_us);
        // velocity: m/s; mass: “parrots”.
        Particle::Id addParticle(Writing, vec3 pos, vec3 velocity, float mass);

    private:
        std::vector<vec3> accelerations;
        int64 debt_us = 0;

        void tick(Stewarding);
        void applyForces(fqsm::Direct<Particle>&);
        void integrate(fqsm::Direct<Particle>&);
        void restoreBases(Stewarding, fqsm::Direct<Particle>&, fqsm::Direct<Atomic>&);
        void applyNails(fqsm::Direct<Particle>&, fqsm::Direct<strong::Nail>&);
    };

}
