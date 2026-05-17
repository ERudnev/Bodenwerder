#pragma once

#include <fQSM/api/interface.h>

// this is "generated" semantic-level domain code 
namespace tests::model {
    using namespace fqsm::api;

    struct SomeEntity : Entity<SomeEntity> {
        struct Quantum { int value; };
    };

    struct SomeComponent : Component<SomeComponent, SomeEntity> {
        struct Quantum { int value; };
    };
}

