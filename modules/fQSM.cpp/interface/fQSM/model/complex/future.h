#pragma once

#include <memory>
#include <vector>

#include <fQSM/erased/future_line.h>
#include <fQSM/erased/patch_line.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/model/complex/state.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/linear/delta.h>

namespace fqsm::model::complex {

    // State seen through a patch. Future lines (and the patch lines they write into) are created
    // on first access to a slot, so a transaction pays only for the aspects it touches.
    class Future : public State {
    public:
        Future(const State& state, ref<Patch> patch, const Rtid::Set& dirty = {});

        template<category::Any Meta>
        linear::Delta<Meta> delta() const;

        // TODO: make it private to hide from workers.
        ref<Patch> patch() { return changes; }
        cref<Patch> patch() const { return fqsm::freeze(changes); }

        Patch::Summary& summary() { return changes->summary; }
        const Patch::Summary& summary() const { return changes->summary; }

        template<category::Any Meta>
        linear::WorkersInterface<Meta>& updates() { return view<Meta>(); }

        template<category::Any Meta>
        const linear::WorkersInterface<Meta>& updates() const { return view<Meta>(); }

        const erased::ReadLine& line(Slot slot) const override { return future(slot); }

    protected:
        erased::FutureLine* future_line(Slot slot) const override { return &future(slot); }

    private:
        erased::FutureLine& future(Slot slot) const;

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
    linear::Delta<Meta> Future::delta() const {
        using Delta = linear::Delta<Meta>;
        const Slot slot = slotOf(TypeId<Meta>);
        const auto mode = dirty.contains(TypeId<Meta>) ? Delta::Mode::dirty : Delta::Mode::clean;
        const erased::PatchLine* patchLine = changes->line(slot);
        return Delta{state.line(slot), patchLine ? *patchLine : erased::empty_patch_line(), mode};
    }
}
