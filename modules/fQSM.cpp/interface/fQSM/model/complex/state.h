#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <fQSM/erased/future_line.h>
#include <fQSM/erased/line.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/view/workers.h>

namespace fqsm::model::complex {

    // Heterogeneous state: one line per schema slot.
    class State {
    public:
        using Slot = intertype::Graph::Slot;

        State(Schema schema, std::shared_ptr<LinePool> pool);
        virtual ~State();
        State(const State&) = delete;
        State& operator=(const State&) = delete;

        virtual const erased::ReadLine& line(Slot slot) const = 0;

        Slot slotOf(meta::Rtid typeId) const { return schema->slotOf(typeId); }
        bool hasLine(meta::Rtid typeId) const { return schema->nodes.contains(typeId); }
        std::size_t quanta() const;

        template<category::Any Meta>
        const ::fqsm::view::Aspect<Meta>& aspect() const { return slot<Meta>(); }

        template<category::Any Meta>
        ::fqsm::view::Aspect<Meta>& aspect() { return slot<Meta>(); }

        const Schema schema;

        // Pool of the Realm this state belongs to.
        const std::shared_ptr<LinePool>& linePool() const { return pool; }

    protected:
        std::unique_ptr<::fqsm::view::SlotBase> release_view(Slot slot) { return std::move(views[slot]); }

        virtual erased::Line* writable_line(Slot) { return nullptr; }
        virtual erased::FutureLine* future_line(Slot) const { return nullptr; }

        // Typed views are created on first use and live as long as this state.
        template<category::Any Meta>
        ::fqsm::view::Slot<Meta>& slot() const;

    private:
        std::unique_ptr<::fqsm::view::SlotBase> make_view_holder(Slot slot) const;

        std::shared_ptr<LinePool> pool;
        mutable std::vector<std::unique_ptr<::fqsm::view::SlotBase>> views;
    };
}

namespace fqsm::model::complex {

    template<category::Any Meta>
    ::fqsm::view::Slot<Meta>& State::slot() const {
        const Slot slot = slotOf(TypeId<Meta>);
        auto& cached = views[slot];
        if (not cached) {
            auto* self = const_cast<State*>(this);
            cached = make_view_holder(slot);
            if (cached)
                cached->rebind(line(slot), self->writable_line(slot), future_line(slot));
            else
                cached = std::make_unique<::fqsm::view::Slot<Meta>>(line(slot), self->writable_line(slot), future_line(slot));
        }
        return static_cast<::fqsm::view::Slot<Meta>&>(*cached);
    }
}
