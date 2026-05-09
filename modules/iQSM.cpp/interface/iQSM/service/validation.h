#pragma once

#include <vector>
#include <iQSM/flow/permit.h>

namespace iqsm::validation {
    // validation of any Aspect means applying Block of functions in defined order
    struct Block {
        using One = void(*)(Writing);
        using Layer = std::vector<One>;

        Layer structural;
        Layer logical;
    };
}