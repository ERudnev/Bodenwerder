#pragma once

#include <fQSM/meta/interface.include.h>

namespace fqsm::state::relations {

    template<aspect::Any Meta>
    struct Anchor : fqsm::Id<Meta> {
    };
    
}