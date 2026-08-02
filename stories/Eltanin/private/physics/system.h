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
    // Linear gravity −Y (9.81); central μ-gravity kept in Settings but not applied.
    // One Dock per tick; hot mutation via Stewarding::direct<T>(); Nail/Gluon seppuku via Writing under Stewarding.
    // Orientation: Horn unit-quaternion method (symmetric N 4×4 + Jacobi), see physics/horn.h.
    // Constraint wave: Atomic / Nail / Gluon `*satisfy(~Particle)` × constraintPasses.
    struct Settings {
        static constexpr float gravity = 9.81f; // m/s², −Y
        static constexpr float centralMu = 8.0f; // unused while linear gravity is on
        static constexpr float constraintStiffness = 0.75f; // Hitman-style goal pull (constraints)
        static constexpr int constraintPasses = 4; // full wave (Horn+Nail+Gluon) per tick
        static constexpr float massMin = 1.0f;
        static constexpr float massMax = 100.0f;
        static constexpr int64 fixedStepUs = 10'000;
        static constexpr float fixedDtS = static_cast<float>(fixedStepUs) / 1'000'000.0f;
        static constexpr float gravitySoften = 0.25f;
        static constexpr float gravitySoften2 = gravitySoften * gravitySoften;
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
        std::vector<vec3> accelerations;
        int64 debt_us = 0;

        void tick(Stewarding);
        void constraintPass(Stewarding);
        void applyForces(fqsm::Direct<Particle>);
        void integrate(fqsm::Direct<Particle>);
    };

}
