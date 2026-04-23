#pragma once

#include <Raidenmamare/scene/node.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::scene {

    using namespace iqsm::q1_gateway;

    struct Light : Attribute<Light, Node>, Require<Node> {
        struct Quantum {
            RGB color;
            float intensity;
            float range;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static auto create(Writing, Pos position, HPB hpb, RGB color, float intensity, float range) -> Id;
        };
        static const Invariants invariants;
    };
}
