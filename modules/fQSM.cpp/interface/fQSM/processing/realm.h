#pragma once

#include <fQSM/state/world.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/transaction.h>
#include <fQSM/processing/permit.h>

namespace fqsm::processing {

    struct Realm : Transaction {
        Realm(const state::world::View& initial) : Transaction(initial), world(initial) {}

        // as Transaction:
        operator Reading() const override { return world; };
        operator Writing() override { return createFork(); }

    private:
        state::world::Data world;

        // create child context
        Permit createFork();

        void update(Context::PatchRef);
    };
}