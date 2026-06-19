#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {

    template<aspect::Any Meta>
    class Reality : State<Meta> {
    public:
        Reality()=default;
        Reality(const Global& initial) : line{}, globalValue(initial) {}
        //Reality(const state::Erased& initial) { _INCOMPLETE_; }

        Items& items() override { return line.at(Rtid::of<Meta>()); }
        const Items& items() override { return line.at(Rtid::of<Meta>()); }
        Global& global() override { return globalValue; }
        const Global& global() const override { return globalValue; }
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