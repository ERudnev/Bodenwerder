#pragma once

#include <eltanin/physics/body.q1.h>

#include <fQSM/api/interface.h>

#include <vector>

namespace eltanin::phys {

    using namespace fqsm::api;

    struct Compound : Attribute<Compound, Body> {
        struct Quantum {
            vector<Body::Id> members;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
