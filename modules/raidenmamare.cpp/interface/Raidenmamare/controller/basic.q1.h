#pragma once

#include <Raidenmamare/scene/node.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::controller {

    using namespace iqsm::dsl_gateway;

    struct Core : Attribute<Core, scene::Node>, Require<scene::Node> {
        struct Quantum {};
        struct Global {
            double clock{};
            vector<bool> keys{};
            index2 mouse{};
            vector<scene::Node::Id> active{};
        };
        struct Operations : OwnTypeOperations {
            static void update(Writing, seconds now_sec);
        };
        static const Invariants invariants;
    };
}
