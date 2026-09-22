#pragma once

#include <eltanin/totality/system.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::totality {

    using namespace fqsm::api;

    struct Celestial : Entity<Celestial> {
        struct Quantum {
            System::Id anchor;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
