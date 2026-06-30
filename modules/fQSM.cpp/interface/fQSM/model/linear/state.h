#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/elementary/state.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    class State : public state::Erased {
    public:
        //using Items = base::cannonball::table::Operational<Id<Meta>, elem::State<Meta>>;
        using Items = base::cannonball::table::Operational<Id<Meta>, Quantum<Meta>>;
        using Global = GlobalValue<Meta>;

        virtual Items& items() = 0;
        virtual const Items& items() const = 0;
        virtual Global& global() = 0;
        virtual const Global& global() const = 0;

        std::size_t quanta() const override { return items().size(); }
    };

    // TODO: remove
    template<category::Any Meta>
    class StateAddressable : State<Meta> {
    };
}