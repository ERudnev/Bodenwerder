#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/state/aspect/erased.h>

namespace fqsm::state::slice {

    template<aspect::Any Meta>
    struct Actual : Interface<Meta>{
        ContainerAbstract& items() override { return actualItems; }
        Global& global() override { return actualGlobal; }

    private:
        using Container = base::DenseTable<Id<Meta>, Quantum<Meta>>;

        Container actualItems;
        Global actualGlobal;
    };

}