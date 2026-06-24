#pragma once

#include <base/cannonball/patch.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    struct Patch : patch::Erased {
        base::cannonball::Patch<Id<Meta>, Quantum<Meta>> items;
        std::optional<GlobalValue<Meta>> global; // nullopt means "no change"

        // experimental (avoiding manipulators:: at all, allowing type-bould batches
        void put_modification(Id<Meta>, Quantum<Meta>);
        void put_deletion(Id<Meta>);
        void put_add(Id<Meta>, Quantum<Meta>);
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

}
