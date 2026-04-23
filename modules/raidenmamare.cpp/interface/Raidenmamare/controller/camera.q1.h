#pragma once

#include <Raidenmamare/controller/dispatcher.q1.h>
#include <Raidenmamare/scene/camera.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::controller {

    using namespace iqsm::q1_gateway;

    struct Camera : Attribute<Camera, Dispatcher>, Require<Dispatcher, scene::Camera> {
        struct Quantum {
            scene::Camera::Id camera;
            index2 previous_mouse;
            bool has_previous;
            double last_step_sec;
            float yaw_rad;
            float pitch_rad;
            bool fps_initialized;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static auto create(Writing, scene::Camera::Id anchor) -> Id;
            static void update(Writing, Id, seconds);
        };
        static const Invariants invariants;
    };
}
