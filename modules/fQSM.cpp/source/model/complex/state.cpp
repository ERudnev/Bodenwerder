#include <fQSM/model/complex/state.h>

namespace fqsm::model::complex {

    State::State(Schema schema, std::shared_ptr<LinePool> pool)
        : schema(schema)
        , pool(std::move(pool))
        , cells(schema->slotCount())
    {}

    ::fqsm::view::Lines State::lines_of(Slot slot) const {
        return ::fqsm::view::Lines{&line(slot), const_cast<State*>(this)->writable_line(slot), future_line(slot)};
    }

    State::~State() = default;

    std::size_t State::quanta() const {
        std::size_t total = 0;
        for (Slot slot = 0; slot < schema->slotCount(); ++slot)
            total += line(slot).size();
        return total;
    }

}
