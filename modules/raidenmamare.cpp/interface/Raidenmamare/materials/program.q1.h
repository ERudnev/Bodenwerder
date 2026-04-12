#pragma once

#include <Raidenmamare/device.q1.h>

#include <GL/glew.h>

#include <iQSM/api/_gateway.h>

namespace rmmr {
namespace material {

    using namespace iqsm::dsl_gateway;

    struct Program : Handle<Program, GLuint, GLuint>, Require<rmmr::Device> {
        struct Materializer : iqsm::resources::Materializer<Program> {
            struct Passport {
                string name;
                string library;
            };

            void materialize(resources::Manager, Reading, Id) const override;
            void release(resources::Manager, Reading, Id) const override;
        };

        struct Quantum {
            const Materializer::Passport passport;
            rmmr::Device::Id device;
        };
        struct Global {};
        struct Operations : OwnTypeOperations {
            static auto vertexFilename(const string& name, const string& library) -> string;
            static auto fragmentFilename(const string& name, const string& library) -> string;

            static void materialize(Reading, Id, resources::Manager);
            static void release(Reading, Id, resources::Manager);
            static auto provide(Reading, Id, resources::Manager) -> RuntimeAccess;
        };
        static const Invariants invariants;
    };
} // namespace material
} // namespace rmmr
