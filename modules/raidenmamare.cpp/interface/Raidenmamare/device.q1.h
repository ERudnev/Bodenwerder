#pragma once

#include <fQSM/api/interface.h>

struct GLFWwindow;

namespace rmmr {

    using namespace fqsm::api;

    struct Window : Entity<Window> {
        using Handle = GLFWwindow*;

        struct Quantum {
            string title;
            index2 size;
            Handle handle;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Device : Entity<Device> {
        struct Quantum {
            string assets_root;
            integer context_major;
            integer context_minor;
            Control<Window> window;
        };
        struct Global {};
        struct Actions : BaseActions {
            static void present(Reading, Id);
            static void poll_events(Reading, Id);
            static void init(
                Writing,
                Id,
                decltype(Window::Quantum::title) title,
                decltype(Window::Quantum::size) size);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };
}
