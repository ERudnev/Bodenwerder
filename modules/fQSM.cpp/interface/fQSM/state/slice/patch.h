#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/state/aspect/erased.h>

namespace fqsm::state::slice {

    template<aspect::Any Meta>
    struct Patch : Erased {
        using Global = GlobalValue<Meta>;
        using Element = std::optional<Quantum<Meta>>;
        using Container = base::TableInterface<Id<Meta>, Quantum<Meta>>;

        Container elements;
        std::optional<Global> global;
    };
}