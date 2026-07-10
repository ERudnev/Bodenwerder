#pragma once

#include <fQSM/api/interface.h>

struct GLFWwindow;

namespace rmmr::system {

    using namespace fqsm::api;

    struct Core : Entity<Core> {
        struct Quantum {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Device : Entity<Device> {
        using Handle = GLFWwindow*;

        struct Quantum {
            Handle handle;
        };
        struct Global {
            string assets_root;
            integer context_major;
            integer context_minor;
        };
        struct Actions : BaseActions {
            static void poll_events(Reading);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Device_group : Group<Device_group, Core, Device> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
