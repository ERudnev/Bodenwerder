#pragma once

#include <memory>
#include <vector>

#include <fQSM/erased/delta.h>
#include <fQSM/erased/future_line.h>
#include <fQSM/erased/patch_line.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/model/complex/state.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/view/delta.h>

namespace fqsm::model::complex {

    // State seen through a patch. Future lines (and the patch lines they write into) are created
    // on first access to a slot, so a transaction pays only for the aspects it touches.
    class Future : public State {
    public:
        Future(const State& state, ref<Patch> patch, const Rtid::Set& dirty = {});
        ~Future() override;

        template<category::Any Meta>
        ::fqsm::view::Delta<Meta> delta() const;

        // TODO: make it private to hide from workers.
        ref<Patch> patch() { return changes; }
        cref<Patch> patch() const { return fqsm::freeze(changes); }

        Patch::Summary& summary() { return changes->summary; }
        const Patch::Summary& summary() const { return changes->summary; }

        template<category::Any Meta>
        ::fqsm::view::WorkersInterface<Meta>& updates() { return slot<Meta>(); }

        template<category::Any Meta>
        const ::fqsm::view::WorkersInterface<Meta>& updates() const { return slot<Meta>(); }

        const erased::ReadLine& line(Slot slot) const override { return future(slot); }
        erased::FutureLine& writer(Slot slot) { return future(slot); }

        const std::vector<erased::InboundIndex>* inbound() const override { return state.inbound(); }
        void pending_layers(Slot slot, std::vector<const erased::PatchLine*>& out) const override {
            if (const auto* line = changes->line(slot)) out.push_back(line);
            state.pending_layers(slot, out);
        }
        bool tainted(Slot slot) const override {
            return dirty.contains(schema->descriptors[slot].id) or state.tainted(slot);
        }

        // Delta of one slot: the base state against this future's patch (dirty when the slot is tainted).
        erased::DeltaCursor delta_begin(Slot slot, erased::DeltaLayer layer) const;
        erased::DeltaCursor delta_end(Slot slot, erased::DeltaLayer layer) const;

    protected:
        erased::FutureLine* future_line(Slot slot) const override { return &future(slot); }

    private:
        erased::FutureLine& future(Slot slot) const;
        const erased::PatchLine& patch_line(Slot slot) const;
        erased::DeltaMode delta_mode(Slot slot) const;

        const State& state; // yep, technically, Future may be Future over Future which is over Future. Be carefull!
        ref<Patch> changes;
        Rtid::Set dirty;
        mutable std::vector<std::unique_ptr<erased::FutureLine>> lines;
    };

    // TODO: make new interface "FutureObservable" and replace this "WorkersInterface"->"Future"
    using WorkersInterface = Future;
}

namespace fqsm::model::complex {

    template<category::Any Meta>
    ::fqsm::view::Delta<Meta> Future::delta() const {
        using Delta = ::fqsm::view::Delta<Meta>;
        const Slot slot = slotOf(TypeId<Meta>);
        const auto mode = dirty.contains(TypeId<Meta>) ? Delta::Mode::dirty : Delta::Mode::clean;
        const erased::PatchLine* patchLine = changes->line(slot);
        return Delta{state.line(slot), patchLine ? *patchLine : erased::empty_patch_line(), mode};
    }
}
