#pragma once

#include <iQSM/api/_gateway.h>

namespace rmmr {

    using namespace iqsm::dsl_gateway;

    struct Program : Binding<Program>, Require<> {
        struct Passport {
            string debugName;
            string vertexFilename;
            string fragmentFilename;
        };
        struct Quantum {
            Passport passport;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {};
        static const Invariants invariants;
    };
}
