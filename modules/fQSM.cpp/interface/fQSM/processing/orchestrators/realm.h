#pragma once

#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/contexts/operational.h>
#include <fQSM/processing/contexts/direct.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/contexts/review.h>
#include <fQSM/processing/transaction.h>

namespace fqsm::processing::orchestrator {

    struct Realm : Transaction {
        Realm(Schema schema) : reality(schema) {}
        Realm(const Realm& other) : Realm(static_cast<const State&>(other)) {} // forcing deep copy
        Realm(const State& other) : reality(other) {}
        // as Transaction:
        operator Reading() const override { return View(reality); }

        template<category::Any Meta>
        operator Direct<Meta>();

        auto result() const -> const review::Result& { return lastResult; }

    private:
        model::complex::Reality reality;
        review::Result lastResult;

        auto writing() -> Writing override;
        auto makeChildPolicy() -> ChildPolicy override;

        void accept(Context::PatchRef);
        void accept_immediate(Rtid::Set dirtyTypes);
    };
}

// Impl:
namespace fqsm::processing::orchestrator {
    template<category::Any Meta>
    Realm::operator Direct<Meta>() {
        auto context = std::make_shared<context::Direct>(context::Direct{
            reality,
            [this](Rtid::Set affected) {
                accept_immediate(std::move(affected));
            },
            {}
        });
        return Direct<Meta>(context);
    }
}