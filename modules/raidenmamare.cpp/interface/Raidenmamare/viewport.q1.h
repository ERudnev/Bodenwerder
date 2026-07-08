#pragma once

#include <Raidenmamare/device.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr {

    using namespace fqsm::api;

    struct Viewport : Entity<Viewport> {
        struct Quantum {
            Device::Id device;
            index2 origin;
            index2 size;
            vec4 clear_color;
        };
        struct Global {};
        struct Actions : BaseActions {
            static void activate(Reading, Id);
            static void clear(Reading, Id);
        };
        struct Internals;
        static const Behavior customAspectReactions() { return {}; }
    };
}
