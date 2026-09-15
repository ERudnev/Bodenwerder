#pragma once

#include <eltanin/physics/body.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;

    struct Resting : Entity<Resting> {
        struct Quantum {
            Body::Id first;
            Body::Id second;
            dvec3 anchorFirst;
            dvec3 anchorSecond;
            vec3 normalFirst;
            dvec3 relativeOffset;
            quat relativeOrientation;
            dvec3 inverseOffset;
            quat inverseOrientation;
            float normalLoad;
            float firstRadius;
            float secondRadius;
            integer firstShape;
            integer secondShape;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
