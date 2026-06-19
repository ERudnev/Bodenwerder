#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {

    template<aspect::Any Meta>
    class Reality : State<Meta> {
        Reality()=default;
        Reality(const Global& initial) : line{}, globalValue(initial) {}
        //Reality(const state::Erased& initial) { _INCOMPLETE_; }

        virtual Items& items() override;
        virtual const Items& items() override;
        virtual Global& global() override;
        virtual const Global& global() const override;
    private:
        Items line;
        GlobalValue<Meta> globalValue;
    };

    template<aspect::Any Meta>
    class StateAddressable : State<Meta> {
    };
}


// impl:
namespace fqsm::model::linear {


}