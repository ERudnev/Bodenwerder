#pragma once

#include <rmmr/scene/camera.q1.h>
#include <rmmr/system/viewInput.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::controller {

    using namespace fqsm::api;

    struct Camera3d : Attribute<Camera3d, scene::Camera> {
        struct Quantum {
            float moveScale;
            system::ViewInput::Id input;
        };
        struct Actions : BaseActions {
            static auto create(Writing, scene::Camera::Id anchor, system::ViewInput::Id input) -> Id;
            // once per frame, input phase: drives every camera from its input mail
            static void tick(Writing, seconds dt);
        };
    };

    // Schema fragment of doctrine/controllers/camera3d.q1: every aspect declared in this file.
    namespace doctrine {
        auto camera3d() -> Schema;
    }

}
