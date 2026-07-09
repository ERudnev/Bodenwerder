#pragma once

#include <fQSM/api/interface.h>

struct GLFWwindow;

namespace rmmr {

    using namespace fqsm::api;

    struct Application : Entity<Application> {
        struct Quantum {};
        struct Global {
            string assets_root;
            integer context_major;
            integer context_minor;
        };
        struct Actions : BaseActions {
            static void poll_events(Reading);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Window : Entity<Window> {
        using Handle = GLFWwindow*;

        struct Quantum {
            string title;
            index2 size;
            Handle handle;
        };
        struct Actions : BaseActions {
            static void present(Reading, Id);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Device : Archetype<Device> {
        static void start(Writing);
        static auto openWindow(Writing, decltype(Window::Quantum::title),  decltype(Window::Quantum::size)) -> Window::Id;
        static void shutdown(Writing);
    };
}
