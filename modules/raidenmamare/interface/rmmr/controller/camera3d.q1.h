#pragma once

#include <rmmr/scene/camera.q1.h>
#include <rmmr/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::controller {

    using namespace fqsm::api;

    struct Camera3d : Attribute<Camera3d, scene::Camera> {
        struct Quantum {};
        struct Actions : BaseActions {
            static auto create(Writing, scene::Camera::Id anchor) -> Id;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
