#pragma once

#include <GL/glew.h>
#include <iQSM/api/_gateway.h>
#include <Raidenmamare/math.q1.h>
#include <Raidenmamare/device.q1.h>

namespace rmmr::primitive {

    using namespace iqsm::dsl_gateway;

    struct OpenGLPrimitive {
        GLuint vao = 0;
        GLuint vbo = 0;
        integer vertex_count = 0;
    };

    struct Base : Handle<Base, OpenGLPrimitive, const OpenGLPrimitive&>, Require<Device> {
        struct Materializer : iqsm::resources::Materializer<Base> {
            struct Passport {
                string debugName;
            };

            void materialize(resources::Manager, Reading, Id) const override;
            void release(resources::Manager, Reading, Id) const override;
        };

        struct Quantum {
            Materializer::Passport passport;
            Device::Id device;
            vector<Pos> vertices;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static void bake(Reading, Id, resources::Manager);
            static void release(Reading, Id, resources::Manager);
            static auto provide(Reading, Id, resources::Manager) -> RuntimeAccess;
        };
        static const Invariants invariants;
    };
}
