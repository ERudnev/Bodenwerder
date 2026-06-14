#pragma once

#include <fQSM/state/composite.h>
#include <fQSM/state/slice/actual.h>

namespace fqsm::state::world {

    struct Actual : Composite<slice::Actual>{
    };
}