#pragma once

#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;
    using Kelvins = float;

    struct Matter {
        dvec3 position;
        float mass;
        Kelvins temperature;
        float cohesion;

        auto hp() const -> float { return mass * cohesion; }
        auto thermalEnergy() const -> float { return mass * temperature; }
    };

    struct Particle : Matter {
        static constexpr float dt = 0.01f;
        dvec3 prev;
        vec3 force;

        auto velocity() const -> dvec3 { return (position - prev) / double(dt); }
        auto impulse() const -> vec3 { return vec3{velocity() * double(mass)}; }
    };

    struct Body : Entity<Body> {
        struct Quantum {
            dvec3 position;
            quat orientation;
            float totalMass;
            float radius;

            auto pose() const -> rmmr::Pose { return rmmr::Pose{.position = vec3{position}, .rotation = orientation}; }
            void pose(rmmr::Pose value) {
                position = dvec3{value.position};
                orientation = value.rotation;
            }
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
