#pragma once

#include <eltanin/physics/body.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;

    struct Resting : Entity<Resting> {
        struct Quantum {
            Body::Id first;
            Body::Id second;
            vec3 anchorFirst;
            vec3 anchorSecond;
            vec3 normalFirst;
            vec3 relativeOffset;
            quat relativeOrientation;
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
