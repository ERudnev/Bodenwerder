#pragma once

#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/review.h>
#include <fQSM/model/complex/data.h>
#include <fQSM/processing/transaction.h>

namespace fqsm::processing {

    struct Realm : Transaction {
        Realm(const View& initial) : world(initial) {}
        // as Transaction:
        operator Reading() const override { return world; }

        // as unique non-transactional data interface:
        template<aspect::Any Meta>
        operator Immediate<Meta>() {
            return {world.slice<Meta>(), [this](aspect::Rtid type) { accept_immediate(type); }};
        }

        auto notes() const -> const Review::Notes& { return lastNotes; }

    private:
        model::complex::State world;
        Review::Notes lastNotes;

        auto writing() -> Writing override;
        auto makeChildPolicy() -> ChildPolicy override;

        void accept(Context::PatchRef);
        void accept_immediate(aspect::Rtid type);
    };
}