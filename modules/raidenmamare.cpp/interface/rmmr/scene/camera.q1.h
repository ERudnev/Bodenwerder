#pragma once

#include <rmmr/scene/node.q1.h>

#include <cstdint>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Camera : Feature<Camera, Node> {
        enum class Mode : std::uint8_t {
            perspective,
            orthographic,
            parallel,
        };
        struct Quantum {
            Mode mode;
            float z_near;
            float z_far;
            float fov_y;
            index2 ortho_size;
        };
        struct Actions : BaseActions {
            static auto projection(Reading, Id, float aspect_ratio) -> mat4;
            static auto view(Reading, Id) -> mat4;
            static auto view_projection(Reading, Id, float aspect_ratio) -> mat4;
            static auto create(Writing, Locator, float fov_y) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
