#pragma once

#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/node.q1.h>
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
            optional<rmmr::scene::Node::Id> sky{};
            optional<rmmr::scene::Camera::Id> camera{};
        };
        struct Actions : BaseActions {
            static void advance(Writing, int64 dt_us);
            static void tetherEnvironment(Writing);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
