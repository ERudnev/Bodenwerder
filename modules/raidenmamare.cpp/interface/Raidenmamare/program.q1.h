#pragma once

#include <Raidenmamare/core.q1.h>

#include <GL/glew.h>

#include <iQSM/api/_gateway.h>

namespace rmmr {

    using namespace iqsm::dsl_gateway;

    struct Program : Handle<Program, GLuint, GLuint>, Require<Core> {
        struct Materializer : iqsm::resources::Materializer<Program> {
            struct Passport {
                string debugName;
                string vertexFilename;
                string fragmentFilename;
            };

            void materialize(resources::Manager, Reading, Id) const override;
            void release(resources::Manager, Reading, Id) const override;
        };

        struct Quantum {
            Materializer::Passport passport;
            Core::Id core;
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
