#pragma once

#include <iQSM/api/_gateway.h>

// allow include of external domain for Resource access?
#include <base/testing/function1f.h>

namespace Q1_iQSM::Etalon {

    using namespace iqsm::q1_gateway;

    struct SampleResource : Handle<SampleResource, base::testing::function1f, const base::testing::function1f&>, Require<> {
        struct Materializer : iqsm::resources::Materializer<SampleResource> {
            struct Passport {
                string description;
                integer cost;
            };

            void materialize(resources::Manager, Reading, Id) const override;
            void release(resources::Manager, Reading, Id) const override;
        };

        struct Quantum {
            const Materializer::Passport passport; // this "const" prevents helpers to modify Passport even in immutable copy-to-change path!
            integer cost_summary;                  // from `element` in DSL; mutable instance state
        };

        struct Global {};

        static const Invariants invariants;

        struct Operations : OwnTypeOperations {
            static auto provide(Reading, Id) -> RuntimeAccess;
            static auto use(Writing, Id, float arg) -> float;
            static auto use_free(Reading, Id, float arg) -> float;
        };
    };

} // namespace Q1_iQSM::Etalon