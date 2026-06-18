#pragma once

#include <unordered_map>

//#include <fQSM/meta/interface.include.h>
#include <base/runtimeTypeId.h>

namespace fqsm::model::composite {

    using TypeId = base::RuntimeTypeId;

    template<typename ErasedContainerType>
    using Container = std::unordered_map<TypeId, ref<ErasedContainerType>>;


}
