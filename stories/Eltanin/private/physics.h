#pragma once

#include <eltanin/physics/atom.q1.h>

#include <fQSM/api/interface.h>

#include <vector>

namespace eltanin::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    // Private physics subsystem (not Q1).
    // Fixed tick 10ms; wall dt accumulates as debt (sub-step remainder).
    // Magic point-mass at origin: a = −μ r / r³ (softened). Spawn circular speed ≈ sqrt(μ/r).
    constexpr float k_central_mu = 8.0f;
    constexpr float k_mass_min = 1.0f;
    constexpr float k_mass_max = 100.0f;

    struct System {
        struct State {
            float time_scale = 1.0f; // wall dt → physics time
        };
        State state;

        void drawUi();
        void step(Stewarding, int64 dt_us);
        // velocity: m/s; mass: “parrots”.
        Atom::Id addParticle(Writing, vec3 pos, vec3 velocity, float mass);

    private:
        std::vector<vec3> accelerations;
        int64 debt_us = 0;

        void tick(Stewarding);
        void applyForces(fqsm::Direct<Atom>&);
        void integrate(fqsm::Direct<Atom>&);
    };

}
