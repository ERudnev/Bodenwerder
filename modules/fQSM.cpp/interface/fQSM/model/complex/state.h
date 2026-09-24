#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <fQSM/erased/future_line.h>
#include <fQSM/erased/line.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/model/linear/state.h>

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
        const linear::State<Meta>& aspect() const { return view<Meta>(); }

        template<category::Any Meta>
        linear::State<Meta>& aspect() { return view<Meta>(); }

        const Schema schema; // defined for Reality/Draft/any homogenous material object

        // Pool of the Realm this state belongs to.
        const std::shared_ptr<LinePool>& linePool() const { return pool; }

    protected:
        std::unique_ptr<linear::state::Erased> release_view(Slot slot) { return std::move(views[slot]); }

        virtual erased::Line* writable_line(Slot) { return nullptr; }
        virtual erased::FutureLine* future_line(Slot) const { return nullptr; }

        // Typed views are created on first use and live as long as this state.
        template<category::Any Meta>
        linear::View<Meta>& view() const;

    private:
        std::unique_ptr<linear::state::Erased> make_view_holder(Slot slot) const;

        std::shared_ptr<LinePool> pool;
        mutable std::vector<std::unique_ptr<linear::state::Erased>> views;
    };
}

namespace fqsm::model::complex {

    template<category::Any Meta>
    linear::View<Meta>& State::view() const {
        const Slot slot = slotOf(TypeId<Meta>);
        auto& cached = views[slot];
        if (not cached) {
            auto* self = const_cast<State*>(this);
            cached = make_view_holder(slot);
            if (cached)
                cached->rebind(line(slot), self->writable_line(slot), future_line(slot));
            else
                cached = std::make_unique<linear::View<Meta>>(line(slot), self->writable_line(slot), future_line(slot));
        }
        return static_cast<linear::View<Meta>&>(*cached);
    }
}
