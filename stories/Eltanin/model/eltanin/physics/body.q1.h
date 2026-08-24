#pragma once

#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;
    using Kelvins = float;

    struct Matter {
        vec3 position;
        float mass;
        Kelvins temperature;
        float cohesion;

        auto hp() const -> float { return mass * cohesion; }
        auto thermalEnergy() const -> float { return mass * temperature; }
    };

    struct Particle : Matter {
        static constexpr float dt = 0.01f;
        vec3 prev;
        vec3 force;

        auto velocity() const -> vec3 { return (position - prev) / dt; }
        auto impulse() const -> vec3 { return velocity() * mass; }
    };

    struct Body : Entity<Body> {
        struct Quantum : Matter {
            quat orientation;
            float radius;
            float hitpoints;

            auto pose() const -> rmmr::Pose { return rmmr::Pose{.position = position, .rotation = orientation}; }
            void pose(rmmr::Pose value) {
                position = value.position;
                orientation = value.rotation;
            }
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
