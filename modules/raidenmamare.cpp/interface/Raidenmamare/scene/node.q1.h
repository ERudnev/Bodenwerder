#pragma once

#include <Raidenmamare/math.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::scene {

    using namespace iqsm::dsl_gateway;

    struct Node : Entity<Node>, Require<> {
        struct Quantum {
            Pos position;
            quat rotation;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static auto create_posHpb(Writing, Pos position, HPB hpb) -> Id;
            static auto transform(Reading, Id) -> mat4;
            static auto hpb(Reading, Id) -> HPB;
            static auto hpb(Writing, Id, HPB hpb) -> void;
        };
        static const Invariants invariants;
    };
}
