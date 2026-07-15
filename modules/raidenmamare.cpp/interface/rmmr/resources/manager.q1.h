#pragma once

#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Manager : Component<Manager, system::Core> {
        struct Quantum {
            filepath location;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Unit : Entity<Unit> {
        struct Quantum {
            Manager::Id manager;
            string name;
            string library;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Unit_group : Group<Unit_group, Manager, Unit> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
