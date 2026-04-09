#pragma once

#include <iQSM/api/_gateway.h>

namespace rmmr {

    using namespace iqsm::dsl_gateway;

    struct Core : Binding<Core>, Require<> {
        struct Passport {
            string assets_root;
        };
        struct Quantum {
            Passport passport;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {};
        static const Invariants invariants;
    };
}
