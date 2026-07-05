#pragma once

#include <fQSM/api/interface.h>

namespace placeholder {

    using namespace fqsm::api;

    struct MyEntity : Entity<MyEntity> {
        struct Quantum {
            int x;
            int y;
        };
        struct Internals;
        struct Reactions : BaseReactions {};
    };
};