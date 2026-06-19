#pragma once

#include <unordered_map>

#include <fQSM/meta/rtid.h>
#include <fQSM/references.h>

namespace fqsm::model {

    template<typename ErasedLineType>
    struct Composite {
        using Container = std::unordered_map<meta::aspect::Rtid, ref<ErasedLineType>, meta::aspect::Rtid::Hash>;

        Container slices;
    };
}

namespace fqsm::model::composite {

    // TODO: add general cast?
}
