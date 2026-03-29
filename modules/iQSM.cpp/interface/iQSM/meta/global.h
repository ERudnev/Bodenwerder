#pragma once

#include <iQSM/references.h>

namespace iqsm::meta {
    template<typename Meta>
    using GlobalData = typename Meta::Global;

    template<typename Meta>
    using Global = cref<GlobalData<Meta>>;

    template<typename Meta>
    Global<Meta> zero_global() {
        using Data = GlobalData<Meta>;
        static const Global<Meta> singleton = base::make_shared<const Data>();
        return singleton;
    }
}

