#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {


    template<aspect::Any Meta>
    class Reality : state::Erased {
        using Container = base::cannonball::table::Operational<Id<Meta>, Quantum<Meta>>

        virtual Container& items();
        virtual const Container& items();
    };

    template<aspect::Any Meta>
    class StateAddressable : State<Meta> {
    };
}