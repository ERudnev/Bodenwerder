#pragma once

#include <Raidenmamare/device.q1.h>
#include <Raidenmamare/scene/node.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::controller {

    using namespace iqsm::dsl_gateway;

    struct Core : Attribute<Core, scene::Node>, Require<scene::Node, ::rmmr::Device> {
        struct Quantum {};
        struct Global {
            optional<Device::Id> device; // Q1: #Device? → iqsm::q1::optional (builtins.h)
            double clock;
            vector<bool> keys;
            index2 mouse;
            vector<scene::Node::Id> active;
        };
        struct Operations : OwnTypeOperations {
            static void update(Writing, seconds now_sec);
        };
        static const Invariants invariants;
    };
}
