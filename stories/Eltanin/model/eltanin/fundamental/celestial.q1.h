#pragma once

#include <eltanin/fundamental/system.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::fundamental {

    using namespace fqsm::api;

    struct Celestial : Entity<Celestial> {
        struct Quantum {
            System::Id anchor;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    namespace doctrine {
        auto celestial() -> Schema;
    }

}
