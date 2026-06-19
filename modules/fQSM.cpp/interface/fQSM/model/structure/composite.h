#pragma once

#include <unordered_map>

#include <fQSM/meta/rtid.h>
#include <fQSM/references.h>

namespace fqsm::model::composite {

    using TypeId = meta::aspect::Rtid;

    template<typename ErasedContainerType>
    using Container = std::unordered_map<TypeId, ref<ErasedContainerType>, meta::aspect::Rtid::Hash>;

}
