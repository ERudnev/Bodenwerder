#pragma once

#include <fQSM/meta/interface.include.h>
#include <base/containers/tableInterface.h>

namespace fqsm::state::slice {

    struct Erased {
        virtual ~Erased()=default;
    };

    template<aspect::Any Meta>
    struct Interface : Erased {
        using Global = GlobalValue<Meta>;
        using ContainerAbstract = base::TableInterface<Id<Meta>, Quantum<Meta>>;

        virtual ContainerAbstract& items()=0;
        virtual Global& global()=0;
    };
}