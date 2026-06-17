#pragma once

#include <unordered_map>

//#include <fQSM/meta/interface.include.h>
#include <base/runtimeTypeId.h>

namespace fqsm::model {

    template<typename BaseType>
    struct Composite {
        using TypeId = base::RuntimeTypeId;
        using Container = std::unordered_map<TypeId, ref<BaseType>>;

        Container lines;
    };

}
