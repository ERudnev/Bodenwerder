#pragma once

#include <eltanin/physics/atomic.q1.h>
#include <eltanin/physics/clast.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <eltanin/physics/strong.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    // Private physics subsystem (not Q1).
    // Fixed tick 10ms; wall dt accumulates as debt (sub-step remainder).
    // No gravity: Verlet + rigid constraints only.
    // One Dock per tick; hot mutation via Stewarding::direct<T>(); Nail/Gluon seppuku via Writing under Stewarding.
    // Orientation: Horn unit-quaternion method (symmetric N 4×4 + Jacobi), see physics/horn.h.
    // Constraint wave: Atomic / Clast / Nail / Gluon `*satisfy(~Particle)` × constraintPasses.
    struct Settings {
        static constexpr float constraintStiffness = 0.75f; // Hitman-style goal pull (constraints)
        static constexpr int constraintPasses = 4;
        static constexpr float massMin = 1.0f;
        static constexpr float massMax = 100.0f;
        static constexpr int64 fixedStepUs = 10'000;
        static constexpr float fixedDtS = static_cast<float>(fixedStepUs) / 1'000'000.0f;
        static constexpr float clueTolerance = 0.01f;
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
        int64 debt_us = 0;

        void tick(Stewarding);
        void constraintPass(Stewarding);
        void integrate(fqsm::Direct<Particle>);
    };

}
