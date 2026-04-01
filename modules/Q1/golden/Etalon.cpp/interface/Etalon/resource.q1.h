#pragma once

#include <iQSM/api/_gateway.h>

namespace Q1CORE::Etalon {

    using namespace iqsm::dsl_gateway;

    struct SampleResource : Binding<SampleResource>, Require<> {
        struct Passport {
            string description;
            integer cost;
        };

        struct Quantum {
            Passport passport;     // implied by `passport { ... }` section in DSL
            integer cost_summary;    // from `element` fields in DSL
        };

        struct Global {};

        static const Invariants invariants;

        struct Operations : OwnTypeOperations {
            static auto use(Writing, Id, resources::Manager, float arg) -> float;
            static auto use_free(Reading, Id, resources::Manager, float arg) -> float;
        };
    };

} // namespace Q1CORE::Etalon