#pragma once

#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/review.h>
#include <fQSM/processing/transaction.h>

namespace fqsm::processing {

    struct Realm : Transaction {
        Realm(Schema schema) : reality(schema) {}
        Realm(const State& other) : reality(other) {}
        // as Transaction:
        operator Reading() const override { return reality; }

        // as unique non-transactional data interface:
        //template<aspect::Any Meta>
        //operator Immediate<Meta>() {
        //    return {reality.aspect<Meta>(), [this](aspect::Rtid type) { accept_immediate(type); }};
        //}
        auto notes() const -> const review::Notes& { return lastNotes; }

    private:
        model::complex::Reality reality;
        review::Notes lastNotes;

        auto writing() -> Writing override;
        auto makeChildPolicy() -> ChildPolicy override;

        void accept(Context::PatchRef);
        void accept_immediate(aspect::Rtid type);
    };
}