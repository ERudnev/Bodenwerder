#pragma once

#include <fQSM/state/world.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/transaction.h>

namespace fqsm::processing {

    struct Realm : Transaction {
        Realm(const View& initial) : world(initial) {}

        // as Transaction:
        operator Reading() const override { return world; };

    private:
        state::world::Data world;

        auto writing() -> Writing override;
        auto makeChildPolicy() -> ChildPolicy override;

        void accept(Context::PatchRef);
    };
}