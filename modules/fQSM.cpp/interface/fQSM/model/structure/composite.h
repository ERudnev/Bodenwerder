#pragma once

#include <unordered_map>

#include <fQSM/meta/rtid.h>
#include <fQSM/references.h>

namespace fqsm::model {

    template<typename ErasedLineType>
    struct Composite {
        using Container = std::unordered_map<meta::Rtid, ref<ErasedLineType>, meta::Rtid::Hash>;

        Container container;
    };
}

namespace fqsm::model::composite {

    // TODO: add general cast?
}
