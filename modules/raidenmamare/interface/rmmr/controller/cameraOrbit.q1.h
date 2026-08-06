#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::controller {

    using namespace fqsm::api;

    struct CameraOrbit : Attribute<CameraOrbit, scene::Camera> {
        struct Quantum {
            Pos pivot;
            HPB hpb;
            float distance;
        };
        struct Actions : BaseActions {
            static auto create(Writing, scene::Camera::Id anchor, Pos pivot, float distance) -> Id;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
