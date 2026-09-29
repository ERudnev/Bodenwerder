#pragma once

#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin {

    using namespace fqsm::api;

    struct World : Entity<World> {
        struct Quantum {};
        struct Global {
            integer step;
            bool paused;
            integer warp;
            optional<rmmr::system::Window::Id> window;
            optional<rmmr::scene::Node::Id> sky;
            optional<rmmr::scene::Node::Id> skyBackdrop;
            optional<rmmr::scene::Camera::Id> camera;
        };
        struct Always {
            static constexpr integer warpTop = 8;
            static auto setup(SettingUp&) -> Global;
            static auto rate(integer warp) -> seconds;
            static auto warpLabel(integer warp) -> const char*;
        };
        struct Actions : BaseActions {
            static void advance(Writing, int64 dt_us);
            static void placeCamera(Writing, rmmr::Pose);
            static void tetherEnvironment(Writing);
            static void pollPauseKey(Writing); // once per frame: rising edge on the bound window toggles warp 0/1
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    namespace doctrine {
        auto world() -> Schema;
    }

}
