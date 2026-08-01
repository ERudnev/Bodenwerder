#pragma once

#include <eltanin/physics/atomic.q1.h>
#include <eltanin/physics/particle.q1.h>

#include <fQSM/api/interface.h>

#include <vector>

namespace eltanin::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    // Private physics subsystem (not Q1).
    // Fixed tick 10ms; wall dt accumulates as debt (sub-step remainder).
    // Magic point-mass at origin: a = −μ r / r³ (softened). Spawn circular speed ≈ sqrt(μ/r).
    // Entire step is Direct (Particle + Atomic) inside one Dock; presentation reacts after commit.
    // Orientation: Horn unit-quaternion method (symmetric N 4×4 + Jacobi), see physics/horn.h.
    constexpr float k_central_mu = 8.0f;
    constexpr float k_fixed_dt_s = 0.01f; // must match System fixed step
    constexpr float k_constraint_stiffness = 0.75f; // Hitman-style goal pull (constraints)
    constexpr float k_mass_min = 1.0f;
    constexpr float k_mass_max = 100.0f;

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
    };

}
