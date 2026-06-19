#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>

namespace fqsm::model::linear {


    template<aspect::Any Meta>
    class State : state::Erased {
        using Items = base::cannonball::table::Operational<Id<Meta>, Quantum<Meta>>;
        using Global = GlobalValue<Meta>;

        virtual Items& items() = 0;
        virtual const Items& items() const = 0;
        virtual Global& global() = 0;
        virtual const Global& global() const = 0;
    };

    // TODO: remove
    template<aspect::Any Meta>
    class StateAddressable : State<Meta> {
    };
}