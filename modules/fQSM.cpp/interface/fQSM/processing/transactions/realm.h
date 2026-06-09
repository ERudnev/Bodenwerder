#pragma once

#include <fQSM/processing/review.h>
#include <fQSM/state/world/data.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/transaction.h>

namespace fqsm::processing {

    struct Realm : Transaction {
        Realm(const View& initial) : world(initial) {}

        // as Transaction:
        operator Reading() const override { return world; };

        auto notes() const -> const Review::Notes& { return lastNotes; }

    private:
        state::world::Data world;
        Review::Notes lastNotes;

        auto writing() -> Writing override;
        auto makeChildPolicy() -> ChildPolicy override;

        void accept(Context::PatchRef);
    };
}