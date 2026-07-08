#pragma once

#include <Raidenmamare/scene/node.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Camera : Attribute<Camera, Node> {
        struct Quantum {
            float fov_y;
            float z_near;
            float z_far;
        };
        struct Global {};
        struct Actions : BaseActions {
            static auto projection(Reading, Id, float aspect_ratio) -> mat4;
            static auto view(Reading, Id) -> mat4;
            static auto view_projection(Reading, Id, float aspect_ratio) -> mat4;
            static auto create(Writing, Locator, float fov_y, float z_near, float z_far) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };
}
