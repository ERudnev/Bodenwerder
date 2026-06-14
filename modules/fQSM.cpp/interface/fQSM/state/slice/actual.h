#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/state/slice/interface.h>

namespace fqsm::state::slice {

    template<aspect::Any Meta>
    struct Actual : slice::Access<Meta> {
        using typename Access<Meta>::Container;
        using typename Access<Meta>::Global;

        Container& items() override { return actualItems; }
        Global& global() override { return actualGlobal; }
    private:
        Container actualItems;
        Global actualGlobal;
    };
}