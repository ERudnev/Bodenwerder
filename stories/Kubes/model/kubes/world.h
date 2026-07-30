#pragma once

#include <rmmr/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace kubes {

    using namespace fqsm::api;

    struct World : Entity<World> {
        struct Quantum {};
        struct Global {
            integer step = 0;
            bool paused = false;
            optional<rmmr::system::Window::Id> window{};
        };
        struct Actions : BaseActions {
            static void advance(Writing, int64 dt_us);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
