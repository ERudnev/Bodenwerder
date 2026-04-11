#pragma once

#include <Raidenmamare/device.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr {

    using namespace iqsm::dsl_gateway;

    struct Viewport : Entity<Viewport>, Require<Device> {
        struct Quantum {
            Device::Id device;
            index2 origin;
            index2 size;
            vec4 clear_color;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static void activate(Reading, Id, resources::Manager);
            static void clear(Reading, Id, resources::Manager);
        };
        static const Invariants invariants;
    };
}
