#pragma once

#include <fQSM/model/_forwards.h>

namespace fqsm::model::linear {


    template<aspect::Any Meta>
    struct State : state::Erased {
    };

    template<aspect::Any Meta>
    struct StateAddressable : State<Meta> {
    };
}