#pragma once

#include <rmmr/scene/camera.q1.h>
#include <rmmr/system/viewInput.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::controller {

    using namespace fqsm::api;

    struct Camera2d : Attribute<Camera2d, scene::Camera> {
        struct Quantum {
            system::ViewInput::Id input;
        };
        struct Actions : BaseActions {
            static auto create(Writing, scene::Camera::Id anchor, system::ViewInput::Id input) -> Id;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
