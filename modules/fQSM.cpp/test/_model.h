#pragma once

#include <fQSM/api/interface.h>

// this is "generated" semantic-level domain code 
namespace tests::model {
    using namespace fqsm::api;

    struct SomeEntity : Entity<SomeEntity> {
        struct Quantum {
            integer value;
        };
        struct Global {
            integer modulus = 2;
        };
        //static const Codex codex;
        struct Service : DefaultService {};
    };

    struct SomeComponent : Component<SomeComponent, SomeEntity>, Require<SomeEntity> {
        struct Quantum {
            string name;
        };
    };

    struct SecondaryAttribute : Attribute<SecondaryAttribute, SomeComponent>, Require<SomeComponent> {
        struct Quantum {
            integer attribute;
        };
    };
}

