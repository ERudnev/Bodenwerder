#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/state/slice/erased.h>
#include <base/containers_deprecated/interface/access.h>

namespace fqsm::state::slice {

    // TODO: consider to remove "Read" layer
    template<aspect::Any Meta>
    struct Read : Erased {
        using Global = GlobalValue<Meta>;
        using Container = base::table::Read<Id<Meta>, Quantum<Meta>>;

        virtual Container& items()=0;
        virtual Global& global()=0;
    };

    template<aspect::Any Meta>
    struct Write : Read<Meta> {
        using Global = GlobalValue<Meta>;
        using Container = base::table::Write<Id<Meta>, Quantum<Meta>>;

        virtual Container& items() override =0;
        virtual Global& global() override =0;
    };

    template<aspect::Any Meta>
    struct Access : Write<Meta> {
        using Global = GlobalValue<Meta>;
        using Container = base::table::Access<Id<Meta>, Quantum<Meta>>;

        virtual Container& items() override =0;
        virtual Global& global() override =0;
    };
}