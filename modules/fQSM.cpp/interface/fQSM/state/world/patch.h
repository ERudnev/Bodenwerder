#pragma once

#include <fQSM/state/composite.h>
#include <fQSM/state/slice/patch.h>

namespace fqsm::state::world {

    struct Patch : Composite<slice::Patch> {
    };
}