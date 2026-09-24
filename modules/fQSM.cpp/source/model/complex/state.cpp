#include <fQSM/model/complex/state.h>

#include <fQSM/model/complex/pool.h>

namespace fqsm::model::complex {

    State::State(Schema schema, std::shared_ptr<LinePool> pool)
        : schema(schema)
        , pool(std::move(pool))
        , views(schema->slotCount())
    {}

    std::unique_ptr<::fqsm::view::SlotBase> State::make_view_holder(Slot slot) const {
        return pool ? pool->take_view(slot) : nullptr;
    }

    State::~State() = default;

    std::size_t State::quanta() const {
        std::size_t total = 0;
        for (Slot slot = 0; slot < schema->slotCount(); ++slot)
            total += line(slot).size();
        return total;
    }

}
