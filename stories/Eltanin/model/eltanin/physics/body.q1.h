#pragma once

#include <base/maybe.h>
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
        dvec3 prev;
        dvec3 force;
    };

    struct Body : Entity<Body> {
        struct Quantum {
            dvec3 position;
            dquat orientation;
            float totalMass;
            float radius;
            Id compound;

            auto pose() const -> rmmr::Pose { return rmmr::Pose{.position = vec3{position}, .rotation = quat{orientation}}; }
            void pose(rmmr::Pose value) {
                position = dvec3{value.position};
                orientation = dquat{value.rotation};
            }
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    inline void bindCohort(Writing context, Body::Id body, Body::Id anchor) {
        with<Body>::modify(context, body)->compound = anchor;
    }

    inline auto createBody(Writing context, Body::Quantum quantum, base::maybe<Body::Id> cohort) -> Body::Id {
        quantum.compound = Body::Id::please_never_use_this_except_patch_rejection_mechanism();
        const auto body = with<Body>::create(context, std::move(quantum));
        bindCohort(context, body, cohort ? *cohort : body);
        return body;
    }

}
