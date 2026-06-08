#pragma once

#include <Raidenmamare/scene/node.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::scene {

    using namespace iqsm::q1_gateway;

    struct Camera : Attribute<Camera, Node> {
        struct Quantum {
            float fov_y;
            float z_near;
            float z_far;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static auto create(Writing, Pos position, HPB hpb, float fov_y, float z_near, float z_far) -> Id;
            static auto projection(Reading, Id, float aspect_ratio) -> mat4;
            static auto view(Reading, Id) -> mat4;
            static auto view_projection(Reading, Id, float aspect_ratio) -> mat4;
        };
        static const Invariants invariants;
    };
}
