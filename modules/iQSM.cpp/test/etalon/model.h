#pragma once

#include <iQSM/api/_gateway.h>

namespace iqsm_internal_model { 
    
    using namespace iqsm::dsl_gateway;

    struct Basic : Entity<Basic>, Require<> {
        struct Quantum {
            string name;
            integer value;
        };
        static const Invariants invariants;
        struct Operations : OwnTypeOperations{
            static auto total_value(Reading)->integer;
            static auto normalized(Reading, Id)->float;
        };
    };

    struct Essentials : Component<Essentials, Basic>, Require<Basic> {
        struct Quantum {
        };
        static const Invariants invariants;
        struct Operations : OwnTypeOperations{};
    };

    struct Optionals : Attribute<Optionals, Basic>, Require<Basic> {
        struct Quantum {
        };
        static const Invariants invariants;
        struct Operations : OwnTypeOperations{};
    };
}