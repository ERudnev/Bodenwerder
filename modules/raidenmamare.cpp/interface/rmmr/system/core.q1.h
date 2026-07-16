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
