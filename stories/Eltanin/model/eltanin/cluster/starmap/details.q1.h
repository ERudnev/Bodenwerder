#pragma once

#include <eltanin/cluster/orbital.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::cluster::starmap {

    using namespace fqsm::api;

    struct Details : Attribute<Details, Axis> {
        struct Quantum {
            string name;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    namespace doctrine {
        auto details() -> Schema;
    }

}
