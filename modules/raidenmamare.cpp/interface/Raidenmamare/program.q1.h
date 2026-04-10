#pragma once

#include <Raidenmamare/core.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr {

    using namespace iqsm::dsl_gateway;

    struct Program : Binding<Program>, Require<Core> {
        struct Passport {
            string debugName;
            string vertexFilename;
            string fragmentFilename;
        };
        struct Quantum {
            Passport passport;
            Core::Id core;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {};
        static const Invariants invariants;
    };
}
