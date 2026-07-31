#pragma once

#include <base/maybe.h>
#include <kubes/physics/atom.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/root.q1.h>

#include <fQSM/api/interface.h>

#include <cmath>
#include <vector>

namespace kubes::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    // Private physics subsystem (not Q1). Bind scene/pack/material after Product setup.
    // Fixed tick 10ms; wall dt accumulates as debt (sub-step remainder).
    // Magic point-mass at origin: a = −μ r / r³ (softened). Spawn circular speed ≈ sqrt(μ/r).
    constexpr float k_central_mu = 8.0f;
    constexpr float k_mass_min = 1.0f;
    constexpr float k_mass_max = 100.0f;

    // Sprite look from mass (log10): keep spawn + Visual::update in sync.
    inline float sprite_scale_for_mass(float mass) {
        const float t = std::log10(std::max(mass, k_mass_min)); // 0 .. ~
        return 0.0125f * (0.55f + 0.75f * t);
    }

    inline RGB sprite_tint_for_mass(float mass) {
        const float t = std::log10(std::max(mass, k_mass_min));
        const float bright = 0.50f + 0.60f * t;
        return RGB{2.8f, 2.2f, 1.1f} * bright;
    }

    struct System {
        struct State {
            bool visualise = true;
            float time_scale = 1.0f; // wall dt → physics time
        };
        State state;

        void bind(scene::Root::Id, resource::sprite::Pack::Id, resource::material::Asset::Id, integer index = 0);
        void drawUi(Writing);
        void step(Stewarding, int64 dt_us);
        // velocity: m/s; mass: “parrots”; tint overrides mass-based colour when set.
        Atom::Id addParticle(Writing, vec3 pos, vec3 velocity, float mass, base::maybe<RGB> tint = {});

    private:
        struct Bound {
            scene::Root::Id scene;
            resource::sprite::Pack::Id pack;
            resource::material::Asset::Id material;
            integer index = 0;
        };

        State prevState{.visualise = true}; // match default: particles already get visuals at spawn
        base::maybe<Bound> bound;
        std::vector<Visual::Id> visuals;
        std::vector<vec3> accelerations;
        int64 debt_us = 0;

        void createVisuals(Writing);
        void destroyVisuals(Writing);
        auto spawnVisual(Writing, Atom::Id, const Atom::Quantum&, RGB tint) -> Visual::Id;
        void tick(Stewarding);
        void applyForces(fqsm::Direct<Atom>&);
        void integrate(fqsm::Direct<Atom>&);
    };

}
