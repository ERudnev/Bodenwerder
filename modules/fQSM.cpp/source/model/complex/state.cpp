#include <fQSM/model/complex/state.h>

namespace fqsm::model::complex {

    State::State(Schema schema)
        : schema(schema)
        , views(schema->slotCount())
    {}

    State::~State() = default;

    std::size_t State::quanta() const {
        std::size_t total = 0;
        for (Slot slot = 0; slot < schema->slotCount(); ++slot)
            total += line(slot).size();
        return total;
    }

}
