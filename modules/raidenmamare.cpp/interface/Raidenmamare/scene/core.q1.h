#pragma once

#include <Raidenmamare/scene/node.q1.h>
#include <Raidenmamare/scene/camera.q1.h>
#include <Raidenmamare/scene/light.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::scene {

    using namespace iqsm::dsl_gateway;

    struct Core : Entity<Core>, Require<Node, Camera, Light> {
        struct Quantum {
            vector<Node::Id> nodes;
            vector<Camera::Id> cameras;
            vector<Light::Id> lights;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {};
        static const Invariants invariants;
    };
}
