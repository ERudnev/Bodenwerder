#pragma once

#include <rmmr/scene/camera.q1.h>
#include <rmmr/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::controller {

    using namespace fqsm::api;

    struct Camera : Attribute<Camera, scene::Camera> {
        struct Quantum {
            float yaw_rad;
            float pitch_rad;
            bool fps_initialized;
        };
        struct Actions : BaseActions {
            static auto create(Writing, scene::Camera::Id anchor) -> Id;
            static void update(Writing, Id, system::Window::Id window);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
