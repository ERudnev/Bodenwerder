#pragma once

#include <iQSM/api/_gateway.h>

// allow include of external domain for Resource access?
#include <base/testing/function1f.h>

namespace Q1CORE::Etalon {

    using namespace iqsm::dsl_gateway;

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
            Materializer::Passport passport;     // implied by `passport { ... }` section in DSL
            integer cost_summary;    // from `element` fields in DSL
        };

        struct Global {};

        static const Invariants invariants;

        struct Operations : OwnTypeOperations {
            static auto provide(Reading, Id, resources::Manager) -> RuntimeAccess;
            static auto use(Writing, Id, resources::Manager, float arg) -> float;
            static auto use_free(Reading, Id, resources::Manager, float arg) -> float;
        };
    };

} // namespace Q1CORE::Etalon