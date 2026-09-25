#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

#include <fQSM/erased/future_line.h>
#include <fQSM/erased/line.h>
#include <fQSM/erased/links.h>
#include <fQSM/erased/patch_line.h>
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
        std::size_t quanta() const;

        // The inbound indexes of the Reality under this state (one per schema link), or nullptr without a Reality.
        virtual const std::vector<erased::InboundIndex>* inbound() const { return nullptr; }
        // The patch lines between this state and its Reality for one slot, topmost first (none for a Reality).
        virtual void pending_layers(Slot, std::vector<const erased::PatchLine*>&) const {}
        // True when the slot was mutated in place somewhere between this state and its Reality.
        virtual bool tainted(Slot) const { return false; }

        template<category::Any Meta>
        const ::fqsm::view::Aspect<Meta>& aspect() const { return slot<Meta>(); }

        template<category::Any Meta>
        ::fqsm::view::Aspect<Meta>& aspect() { return slot<Meta>(); }

        const Schema schema;

        // Pool of the Realm this state belongs to.
        const std::shared_ptr<LinePool>& linePool() const { return pool; }

    protected:
        virtual erased::Line* writable_line(Slot) { return nullptr; }
        virtual erased::FutureLine* future_line(Slot) const { return nullptr; }

        // Typed views are built on first use in a cell and live as long as this state.
        template<category::Any Meta>
        ::fqsm::view::Slot<Meta>& slot() const;

    private:
        struct Cell {
            alignas(::fqsm::view::Lines) std::byte storage[sizeof(::fqsm::view::Lines)];
            bool built = false;
        };

        ::fqsm::view::Lines lines_of(Slot slot) const;

        std::shared_ptr<LinePool> pool;
        mutable std::vector<Cell> cells;
    };
}

namespace fqsm::model::complex {

    template<category::Any Meta>
    ::fqsm::view::Slot<Meta>& State::slot() const {
        using View = ::fqsm::view::Slot<Meta>;
        static_assert(sizeof(View) == sizeof(::fqsm::view::Lines) and std::is_trivially_destructible_v<View>);
        const Slot slot = slotOf(TypeId<Meta>);
        auto& cell = cells[slot];
        if (not cell.built) {
            ::new (static_cast<void*>(cell.storage)) View(lines_of(slot));
            cell.built = true;
        }
        return *std::launder(reinterpret_cast<View*>(cell.storage));
    }
}
