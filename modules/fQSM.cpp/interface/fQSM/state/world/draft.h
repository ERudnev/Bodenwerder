#pragma once

#include <fQSM/state/_forwards.h>

namespace fqsm::state::world {

    struct Draft {
        const world::Actual& world;
        world::Patch& patch;
    };
}