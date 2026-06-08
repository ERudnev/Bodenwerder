#pragma once

#include <Raidenmamare/device.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr {

    using namespace iqsm::q1_gateway;

    struct Viewport : Entity<Viewport> {
        struct Quantum {
            Device::Id device;
            index2 origin;
            index2 size;
            vec4 clear_color;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static void activate(Reading, Id);
            static void clear(Reading, Id);
        };
        static const Invariants invariants;
    };
}
