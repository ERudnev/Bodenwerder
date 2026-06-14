#pragma once

#include <base/containers/patch.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/state/slice/interface.h>

namespace fqsm::state::slice {

    template<aspect::Any Meta>
    struct Patch : Erased {
        using Global = GlobalValue<Meta>;
        using Element = base::patch::Element<Quantum<Meta>>;
        using Container = base::Patch<Id<Meta>, Element>;

        Container elements;
        std::optional<Global> global;
    };
}