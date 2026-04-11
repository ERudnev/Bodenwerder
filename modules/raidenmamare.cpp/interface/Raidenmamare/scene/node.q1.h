#pragma once

#include <iQSM/api/_gateway.h>

namespace rmmr::scene {

    using namespace iqsm::dsl_gateway;

    struct Node : Entity<Node>, Require<> {
        struct Quantum {
            vec3 translation;
            quat rotation;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static auto transform(Reading, Id) -> mat4;
        };
        static const Invariants invariants;
    };
}
