#pragma once

#include <fQSM/api/interface.h>

struct GLFWwindow;

namespace rmmr::system {

    using namespace fqsm::api;

    struct Core : Entity<Core> {
        struct GLVer {
            integer major;
            integer minor;
        };

        struct Quantum {
            filepath assets_root;
            GLVer version;
        };
        struct Global {
            optional<Id> singleton{};
        };
        struct Actions : BaseActions {
            // For change analysis (Clock deltas keyed by Core id).
            static auto singleton(Reading) -> optional<Id>;
            // Current Core quantum, if the singleton is alive.
            static auto access(Reading) -> const Quantum*;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Clock : Component<Clock, Core> {
        struct Quantum {
            int64 absolute; // microseconds since process start
        };
        struct Global {
            optional<Id> singleton{};
        };
        struct Actions : BaseActions {
            static auto singleton(Reading) -> optional<Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Device : Entity<Device> {
        using Handle = GLFWwindow*;

        struct Quantum {
            Anchor<Core> core;
            Handle handle;
        };
        struct Actions : BaseActions {
            static void poll_events(Reading);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
