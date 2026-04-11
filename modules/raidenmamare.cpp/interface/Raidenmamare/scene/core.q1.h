#pragma once

#include <Raidenmamare/scene/node.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::scene {

    using namespace iqsm::dsl_gateway;

    struct Core : Entity<Core>, Require<Node> {
        struct Quantum {
            vector<Node::Id> nodes;
        };
        struct Global {};
        static const Invariants invariants;
    };
}
