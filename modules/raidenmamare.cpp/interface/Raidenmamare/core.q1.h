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
                integer width;
                integer height;
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
            static void open(Reading, Id, resources::Manager);
            static void close(Reading, Id, resources::Manager);
            static auto provide(Reading, Id, resources::Manager) -> RuntimeAccess;
        };
        static const Invariants invariants;
    };
}
