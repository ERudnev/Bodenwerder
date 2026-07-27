#pragma once

#include <base/cannonball/future.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/state.h>
#include <fQSM/model/linear/workersInterface.h>

#include <format>
#include <stdexcept>

namespace fqsm::model::linear {

    template<category::Any Meta>
    class Future : public State<Meta>, public WorkersInterface<Meta> {
    public:
        using Items = State<Meta>::Items;
        using Global = State<Meta>::Global;

        Future(const linear::State<Meta>& state, ref<linear::Patch<Meta>> patch)
            : draftItems(state.items(), patch->items, base::cannonball::SeeChanges::observable)
            , futureGlobal(state.global(), patch->global)
        {}

        virtual Items& items() override { return draftItems; }
        virtual const Items& items() const override { return draftItems; }
        virtual Global& global() override { return futureGlobal.access(); }
        virtual const Global& global() const override { return futureGlobal.get(); }

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

        base::cannonball::Future<Id<Meta>, Quantum<Meta>> draftItems;
        FutureGlobal futureGlobal;
    };
}

namespace fqsm::model::linear {

    template<category::Any Meta>
    void Future<Meta>::put_modification(Id<Meta> id, Quantum<Meta> value) {
        draftItems.patch.modify(std::move(id), std::move(value));
    }

    template<category::Any Meta>
    void Future<Meta>::put_deletion(Id<Meta> id) {
        if (auto* patched = draftItems.patch.find(id)) {
            draftItems.patch.insert(std::move(id), Patchlet::deletion(std::move(patched->quantum)));
            return;
        }
        if (const auto* current = draftItems.state.find(id)) {
            draftItems.patch.insert(std::move(id), Patchlet::deletion(*current));
            return;
        }
    }

    template<category::Any Meta>
    void Future<Meta>::put_add(Id<Meta> id, Quantum<Meta> value) {
        draftItems.patch.insert(std::move(id), Patchlet::modification(std::move(value)));
    }

    template<category::Any Meta>
    void Future<Meta>::put_global(GlobalValue<Meta> value) {
        futureGlobal.patchGlobal = {std::move(value)};
    }

    template<category::Any Meta>
    Quantum<Meta>& Future<Meta>
    ::get_modification_access(Id<Meta> id) {
        auto patchEntry = draftItems.patch.find(id);
        if (not patchEntry) {
            const auto* current = draftItems.state.find(id);
            if (not current) {
                throw std::runtime_error(std::format(R"(cannot modify "{}" {}: not present)", Rtid::name<Meta>(), id));
            }
            // touch/ensure: unverified patchlet; old taken from state
            return draftItems.patch.insert(id, Patchlet::possible(*current)).quantum;
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