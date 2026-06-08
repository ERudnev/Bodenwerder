#pragma once

#include <Raidenmamare/scene/node.q1.h>
#include <Raidenmamare/primitives/base.q1.h>
#include <Raidenmamare/materials/core.q1.h>
#include <Raidenmamare/math.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::scene {

    using namespace iqsm::q1_gateway;

    struct PrimitiveActor : Attribute<PrimitiveActor, Node> {
        struct Quantum {
            primitive::Base::Id geometry;
            material::Core::Id material;
            RGB albedo;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static auto create(Writing, Pos, HPB, primitive::Base::Id geometry, material::Core::Id, RGB albedo) -> Id;
        };
        static const Invariants invariants;
    };
}
