#pragma once

#include <base/cannonball/patch.h>
#include <base/function_ref.h>
#include <base/shared_reference.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    struct WorkersInterface { // Zag Zag!
        virtual ~WorkersInterface()=default;

        virtual void put_modification(Id<Meta>, Quantum<Meta>) = 0;
        virtual void put_deletion(Id<Meta>) = 0;
        virtual void put_add(Id<Meta>, Quantum<Meta>) = 0;
        virtual void put_as_restored(Id<Meta>, Quantum<Meta>) = 0;
        virtual void put_global(GlobalValue<Meta>) = 0;
        virtual Quantum<Meta>& update_modification(Id<Meta>, base::function_ref<const Quantum<Meta>&()> prepatch) = 0;
        virtual GlobalValue<Meta>& update_global(base::function_ref<const GlobalValue<Meta>&()> prepatch) = 0;
    };

    template<category::Any Meta>
    struct Patch : patch::Erased, WorkersInterface<Meta> {
        // TODO: make private with const access (give monopoly to "put_.." functions?
        // or.. remove "put_..." functions :)
        // or.. make "PatchAssemblyInterface"
        base::cannonball::Patch<Id<Meta>, Quantum<Meta>> items;
        std::optional<GlobalValue<Meta>> global; // nullopt means "no change"

        bool has_changes() const override { return not items.empty() or global.has_value(); }
        void absorb(const Patch&);
        void clear();

        // experimental (avoiding manipulators:: at all, allowing type-bould batches
        void put_modification(Id<Meta>, Quantum<Meta>) override;
        void put_deletion(Id<Meta>) override;
        void put_add(Id<Meta>, Quantum<Meta>) override;
        void put_as_restored(Id<Meta>, Quantum<Meta>) override; // persistency gate
        void put_global(GlobalValue<Meta>) override;
        Quantum<Meta>& update_modification(Id<Meta>, base::function_ref<const Quantum<Meta>&()> prepatch) override;
        GlobalValue<Meta>& update_global(base::function_ref<const GlobalValue<Meta>&()> prepatch) override;

        // schema
        static ref<patch::Erased> create() { return base::make_shared<Patch<Meta>>(); }
    };
}

// Impl
namespace fqsm::model::linear {

    template<category::Any Meta>
    void Patch<Meta>::put_modification(Id<Meta> id, Quantum<Meta> value) {
        items.modify(std::move(id), std::move(value));
    }

    template<category::Any Meta>
    void Patch<Meta>::put_deletion(Id<Meta> id) {
        items.insert(std::move(id), std::nullopt);
    }

    template<category::Any Meta>
    void Patch<Meta>::put_add(Id<Meta> id, Quantum<Meta> value) {
        items.insert(std::move(id), std::move(value));
    }

    template<category::Any Meta>
    void Patch<Meta>::put_as_restored(Id<Meta> id, Quantum<Meta> value) {
        // persistency gate: technically is identical to put_add()
        items.insert(std::move(id), std::move(value));
    }


    template<category::Any Meta>
    void Patch<Meta>::put_global(GlobalValue<Meta> value) {
        global = {std::move(value)};
    }

    template<category::Any Meta>
    Quantum<Meta>& Patch<Meta>
    ::update_modification(Id<Meta> id, base::function_ref<const Quantum<Meta>&()> prepatch) {
        return items.modify_modification(std::move(id), prepatch);
    }

    template<category::Any Meta>
    GlobalValue<Meta>& Patch<Meta>
    ::update_global(base::function_ref<const GlobalValue<Meta>&()> prepatch) {
        if (not global.has_value()) {
            global = prepatch();
        }
        return *global;
    }

    template<category::Any Meta>
    void Patch<Meta>::absorb(const Patch& other) {
        if (other.global.has_value()) global = other.global;
        base::cannonball::Patch<Id<Meta>, Quantum<Meta>>::merge(items, other.items);
    }

    template<category::Any Meta>
    void Patch<Meta>::clear() {
        items.clear();
        global.reset();
    }

}
