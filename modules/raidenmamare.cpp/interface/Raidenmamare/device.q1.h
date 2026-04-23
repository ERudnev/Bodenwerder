#pragma once

#include <iQSM/api/_gateway.h>

struct GLFWwindow;

namespace rmmr {

    using namespace iqsm::q1_gateway;

    struct Device : Handle<Device, GLFWwindow*, GLFWwindow*>, Require<> {
        struct Materializer : iqsm::resources::Materializer<Device> {
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
            const Materializer::Passport passport;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static void present(Reading, Id);
            static void poll_events(Reading, Id);
            static void materialize(Reading, Id, resources::Manager);
            static void release(Reading, Id, resources::Manager);
            static auto provide(Reading, Id) -> RuntimeAccess;
        };
        static const Invariants invariants;
    };
}
