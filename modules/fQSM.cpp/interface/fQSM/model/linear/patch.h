#pragma once

#include <base/cannonball/patch.h>
#include <base/shared_reference.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    struct Patch : patch::Erased {
        // TODO: make private with const access (give monopoly to "put_.." functions?
        // or.. remove "put_..." functions :)
        // or.. make "PatchAssemblyInterface"
        base::cannonball::Patch<Id<Meta>, Quantum<Meta>> items;
        std::optional<GlobalValue<Meta>> global; // nullopt means "no change"

        bool has_changes() const override { return not items.empty() or global.has_value(); }
        void absorb(const Patch&);
        void clear();

        // experimental (avoiding manipulators:: at all, allowing type-bould batches
        void put_modification(Id<Meta>, Quantum<Meta>);
        void put_deletion(Id<Meta>);
        void put_add(Id<Meta>, Quantum<Meta>);

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
