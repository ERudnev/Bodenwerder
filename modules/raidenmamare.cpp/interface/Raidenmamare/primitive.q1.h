#pragma once

#include <Raidenmamare/core.q1.h>

#include <GL/glew.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::primitive {

    using namespace iqsm::dsl_gateway;

    struct OpenGLPrimitive {
        GLuint vao = 0;
        GLuint vbo = 0;
        integer vertex_count = 0;
    };

    struct Base : Handle<Base, OpenGLPrimitive, const OpenGLPrimitive&>, Require<Core> {
        struct Materializer : iqsm::resources::Materializer<Base> {
            struct Passport {
                string debugName;
            };

            void materialize(resources::Manager, Reading, Id) const override;
            void release(resources::Manager, Reading, Id) const override;
        };

        struct Quantum {
            Materializer::Passport passport;
            Core::Id core;
            vector<vec3> vertices;
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
