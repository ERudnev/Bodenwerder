#pragma once

#include <base/cannonball/patch.h>
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
        virtual void put_global(GlobalValue<Meta>) = 0;
        virtual Quantum<Meta>& update_modification(Id<Meta>, const Quantum<Meta>& providedPrepatchData) = 0;
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
        void put_global(GlobalValue<Meta>) override;
        Quantum<Meta>& update_modification(Id<Meta>, const Quantum<Meta>& providedPrepatchData) override;

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
    void Patch<Meta>::put_global(GlobalValue<Meta> value) {
        global = {std::move(value)};
    }

    template<category::Any Meta>
    Quantum<Meta>& Patch<Meta>
    ::update_modification(Id<Meta> id, const Quantum<Meta>& providedPrepatchData) {
        return items.modify_modification(id, providedPrepatchData);
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
