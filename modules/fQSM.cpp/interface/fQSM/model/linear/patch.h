#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>

namespace fqsm::model::linear {

    template<aspect::Any Meta>
    struct Patch : patch::Erased {
    };
}
