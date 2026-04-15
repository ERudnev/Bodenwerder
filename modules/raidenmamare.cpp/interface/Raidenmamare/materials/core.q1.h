#pragma once

#include <Raidenmamare/materials/program.q1.h>

#include <Raidenmamare/materials/uniformSemantics.h>

#include <iQSM/api/_gateway.h>


namespace rmmr::internals::material {
    struct CoreCompiled {
        const rmmr::material::Program::RuntimeAccess program;
        const rmmr::material::Semantics::RuntimeMapping locations;
        const vector<rmmr::material::Semantics::Binding> bindings;
    };
}

namespace rmmr::material {

    using namespace iqsm::dsl_gateway;

    struct Core : Handle<Core, rmmr::internals::material::CoreCompiled, const rmmr::internals::material::CoreCompiled&>, Require<Program>
    {
        struct Materializer : iqsm::resources::Materializer<Core> {
            struct Passport {
                Program::Id program;
                vector<Semantics::PersistentId> uniforms;
            };

            void materialize(resources::Manager, Reading, Id) const override;
            void release(resources::Manager, Reading, Id) const override;
        };

        struct Quantum {
            const Materializer::Passport passport;
            string name;
        };
        struct Global {};
        
        static const Invariants invariants;

        struct Operations : OwnTypeOperations {
            static auto uniformIds(const vector<string>& names) -> vector<Semantics::PersistentId>;
            static auto provide(Reading, Id) -> RuntimeAccess;
            static auto apply(Reading, Id, rmmr::Device::Id) -> RuntimeAccess;
        };
    };
}
