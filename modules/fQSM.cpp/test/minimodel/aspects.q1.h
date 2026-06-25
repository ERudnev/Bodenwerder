#pragma once

#include <fQSM/api/interface.h>

// this is "generated" semantic-level domain code
namespace tests::model {
    using namespace fqsm::api;

    struct SomeEntity : Entity<SomeEntity> {
        struct Quantum {
            integer value;
            //optional<anchor<SomeEntity>> parent;
        };
        struct Global {
            integer modulus = 2;
        };
        static const Behavior custom;
        struct Actions : BaseActions {
            struct Private;
            static auto constantFunc(Reading, Id)->integer;
        };
    };

    struct SomeComponent : Component<SomeComponent, SomeEntity> {
        struct Quantum {
            string name;
        };
        static const Behavior custom;
        struct Actions : BaseActions {};
    };

    struct SecondaryAttribute : Attribute<SecondaryAttribute, SomeComponent> {
        struct Quantum {
            integer attribute;
        };
        static const Behavior custom;
        struct Actions : BaseActions {};
    };
}
