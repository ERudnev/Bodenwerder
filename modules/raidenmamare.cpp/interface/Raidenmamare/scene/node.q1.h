#pragma once

#include <iQSM/api/_gateway.h>

namespace rmmr::scene {

    using namespace iqsm::dsl_gateway;

    struct Node : Entity<Node>, Require<> {
        struct Quantum {
            vec3 position;
            quat rotation;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static auto transform(Reading, Id) -> mat4;
            static auto euler(Reading, Id) -> vec3;
            static auto euler(Writing, Id, vec3 heading_pitch_bank) -> void;
        };
        static const Invariants invariants;
    };
}
