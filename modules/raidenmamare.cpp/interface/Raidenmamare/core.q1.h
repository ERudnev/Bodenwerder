#pragma once

#include <iQSM/api/_gateway.h>

struct GLFWwindow;

namespace rmmr {

    using namespace iqsm::dsl_gateway;

    struct Core : Handle<Core, GLFWwindow*, GLFWwindow*>, Require<> {
        struct Materializer : iqsm::resources::Materializer<Core> {
            struct Passport {
                string assets_root;
                string title;
                index2 size;
                integer context_major;
                integer context_minor;
            };

            void materialize(resources::Manager, Reading, Id) const override;
            void release(resources::Manager, Reading, Id) const override;
        };

        struct Quantum {
            Materializer::Passport passport;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static void present(Reading, Id, resources::Manager);
            static void poll_events(Reading, Id, resources::Manager);
            static void materialize(Reading, Id, resources::Manager);
            static void release(Reading, Id, resources::Manager);
            static auto provide(Reading, Id, resources::Manager) -> RuntimeAccess;
        };
        static const Invariants invariants;
    };
}
