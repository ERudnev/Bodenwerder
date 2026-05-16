#pragma once

#include <fQSM/api/interface.h>

// this is "generated" semantic-level domain code 
namespace tests::generated_domain {
    using namespace fqsm::api;

    struct SomeEntity : Entity<SomeEntity> {
        struct Quantum { int value; };
    };

    struct SomeComponent : Component<SomeComponent, SomeEntity> {
        struct Quantum { int value; };
    };

}

// this is "facade" domain
namespace tests::model {
    using namespace fqsm::api;
    using SomeEntity = Register<generated_domain::SomeEntity>;
    using SomeComponent = Register<generated_domain::SomeComponent>;
}

