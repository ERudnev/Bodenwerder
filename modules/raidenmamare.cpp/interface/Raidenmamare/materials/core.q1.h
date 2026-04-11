#pragma once

#include <Raidenmamare/materials/program.q1.h>

#include <Raidenmamare/materials/uniformSemantics.h>

#include <iQSM/api/_gateway.h>


namespace rmmr::internals::material {
    struct CoreCompiled {
        const rmmr::material::Program::RuntimeAccess program;
        const Semantics::RuntimeMapping locations;
    };
}

namespace rmmr::material {

    using namespace iqsm::dsl_gateway;

    struct Core : Handle<Core, rmmr::internals::material::CoreCompiled, const rmmr::internals::material::CoreCompiled&>, Require<Program>
    {
        struct Operations : OwnTypeOperations {
            static auto uniformIds(const vector<string>& names) -> vector<Semantics::PersistentId>;
        };

        struct Materializer : iqsm::resources::Materializer<Core> {
            struct Passport {
                Program::Id program;
                vector<Semantics::PersistentId> uniforms;
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
