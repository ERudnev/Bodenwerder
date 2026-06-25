#include <fQSM/model/complex/state.h>

namespace fqsm::model::complex {

    std::size_t State::quanta() const {
        std::size_t total = 0;
        for (const auto& entry : composition().container)
            total += entry.second->quanta();
        return total;
    }

}
