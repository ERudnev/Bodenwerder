#pragma once

#include <optional>
#include <utility>

#include <base/cannonball/patchlet.h>
#include <fQSM/erased/overlay.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/linear/state.h>
#include <fQSM/model/linear/workersInterface.h>
#include <fQSM/utility/messages.h>


namespace fqsm::model::linear {

    template<category::Any Meta>
    class Future final : public State<Meta>, public WorkersInterface<Meta> {
    public:
        using Items = State<Meta>::Items;
        using Global = State<Meta>::Global;

        Future(const linear::State<Meta>& state, ref<linear::Patch<Meta>> patch)
            : origin(state)
            , changes(*patch)
            , overlay(state.line(), patch->view())
            , view(overlay)
            , futureGlobal{state.global(), patch->global}
        {}

        Future(const Future&) = delete;
        Future& operator=(const Future&) = delete;

        Items& items() override { return view; }
        const Items& items() const override { return view; }
        Global& global() override { return futureGlobal.access(); }
        const Global& global() const override { return futureGlobal.get(); }
        const erased::ReadLine& line() const override { return overlay; }

         // WorkersInterface
         void put_modification(Id<Meta>, Quantum<Meta>) override;
         void put_deletion(Id<Meta>) override;
         void put_add(Id<Meta>, Quantum<Meta>) override;
         void put_global(GlobalValue<Meta>) override;
         Quantum<Meta>& get_modification_access(Id<Meta>) override;
         GlobalValue<Meta>& get_access_global() override;

    private:
        using Patchlet = base::cannonball::Patchlet<Quantum<Meta>>;
        struct FutureGlobal {
            const Global& stateGlobal;
            std::optional<Global>& patchGlobal;
            const Global& get() const {
                if (patchGlobal) return *patchGlobal;
                return stateGlobal;
            }
            Global& access() {
                if (not patchGlobal)
                    patchGlobal = stateGlobal;
                return *patchGlobal;
            }
        };

        const State<Meta>& origin;
        Patch<Meta>& changes;
        erased::Overlay overlay;
        Items view;
        FutureGlobal futureGlobal;
    };
}

namespace fqsm::model::linear {

    template<category::Any Meta>
    void Future<Meta>::put_modification(Id<Meta> id, Quantum<Meta> value) {
        changes.items.modify(std::move(id), std::move(value));
    }

    template<category::Any Meta>
    void Future<Meta>::put_deletion(Id<Meta> id) {
        if (auto* patched = changes.items.find(id)) {
            changes.items.insert(std::move(id), Patchlet::deletion(std::move(patched->quantum)));
            return;
        }
        if (const auto* current = origin.items().find(id)) {
            changes.items.insert(std::move(id), Patchlet::deletion(*current));
            return;
        }
    }

    template<category::Any Meta>
    void Future<Meta>::put_add(Id<Meta> id, Quantum<Meta> value) {
        changes.items.insert(std::move(id), Patchlet::modification(std::move(value)));
    }

    template<category::Any Meta>
    void Future<Meta>::put_global(GlobalValue<Meta> value) {
        futureGlobal.patchGlobal = {std::move(value)};
    }

    template<category::Any Meta>
    Quantum<Meta>& Future<Meta>
    ::get_modification_access(Id<Meta> id) {
        auto patchEntry = changes.items.find(id);
        if (not patchEntry) {
            const auto* current = origin.items().find(id);
            if (not current) {
                utility::messages::throw_not_present("cannot modify", Rtid::name<Meta>(), id.raw());
            }
            // touch: unverified patchlet, old value taken from the base
            return changes.items.insert(id, Patchlet::possible(*current)).quantum;
        }
        return patchEntry->quantum;
    }

    template<category::Any Meta>
    GlobalValue<Meta>& Future<Meta>
    ::get_access_global() {
        if (not futureGlobal.patchGlobal) {
            futureGlobal.patchGlobal = futureGlobal.stateGlobal;
        }
        return *futureGlobal.patchGlobal;
    }
}
