#pragma once

#include <Raidenmamare/materials/program.q1.h>

#include <iQSM/api/_gateway.h>


namespace rmmr::internals::material {
    struct TypeCompiled {
        //GLuint program = 0;
        rmmr::material::Program::RuntimeAccess program = 0;
    };
}

namespace rmmr::material {

    using namespace iqsm::dsl_gateway;

    struct Type : Handle<Type, rmmr::internals::material::TypeCompiled, const rmmr::internals::material::TypeCompiled&>, Require<Program>
    {
        struct Materializer : iqsm::resources::Materializer<Type> {
            struct Passport {
                Program::Id program;
            };

            void materialize(resources::Manager, Reading, Id) const override;
            void release(resources::Manager, Reading, Id) const override;
        };

        struct Quantum {
            Materializer::Passport passport;
            string name;
        };
        struct Global {};
        static const Invariants invariants;
    };
}
