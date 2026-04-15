#pragma once

#include <GL/glew.h>
#include <iQSM/api/_gateway.h>
#include <Raidenmamare/math.q1.h>
#include <Raidenmamare/device.q1.h>
#include <Raidenmamare/primitives/geometrySemantics.h>

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
                // Explicit vertex layout contract (channel semantics IDs).
                // Interleaved buffer: position-only (attrib 0), or position + normal (attribs 0–1).
                vector<GeometrySemantics::PersistentId> layout;
            };

            void materialize(resources::Manager, Reading, Id) const override;
            void release(resources::Manager, Reading, Id) const override;
        };

        struct Quantum {
            const Materializer::Passport passport;
            Device::Id device;
            vector<Pos> positions;
            vector<Pos> normals;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static void bake(Reading, Id, resources::Manager);
            static void release(Reading, Id, resources::Manager);
            static auto provide(Reading, Id) -> RuntimeAccess;
        };
        static const Invariants invariants;
    };
}
