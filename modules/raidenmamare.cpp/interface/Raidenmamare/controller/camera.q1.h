#pragma once

#include <Raidenmamare/scene/camera.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::controller {

    using namespace fqsm::api;

    struct Camera : Attribute<Camera, scene::Camera> {
        struct Quantum {
            index2 previous_mouse;
            bool has_previous;
            double last_step_sec;
            float yaw_rad;
            float pitch_rad;
            bool fps_initialized;
        };
        struct Actions : BaseActions {
            static auto create(Writing, scene::Camera::Id anchor) -> Id;
            static void update(Writing, Id, seconds now);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
