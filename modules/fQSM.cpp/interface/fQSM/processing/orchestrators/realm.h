#pragma once

#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/contexts/operational.h>
#include <fQSM/processing/contexts/direct.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/contexts/review.h>
#include <fQSM/processing/transaction.h>
#include <fQSM/processing/orchestrators/branch.h>

namespace fqsm::processing::orchestrator {

    struct Realm : Transaction {
        Realm(Schema schema) : reality(schema) {}
        //Realm(const Realm& other) : Realm(static_cast<const State&>(other)) {} // forcing deep copy
        Realm(const Realm& other) : reality(static_cast<const State&>(other.reality)) {} // TODO clarify me
        Realm(const State& other) : reality(other) {}

        // running local transaction (currently syncronous):
        template<typename F>
        auto branch(F&& worker) -> std::invoke_result_t<F, Writing> { Branch context(*this); return std::invoke(std::forward<F>(worker), static_cast<Writing>(context)); }

        operator Reading() const override { return View(reality); }
        const model::complex::State* operator->() const { return &reality; }

        operator Stewarding();

        auto result() const -> const model::complex::Patch::Result& { return lastResult; }

    private:
        model::complex::Reality reality;
        model::complex::Patch::Result lastResult;

        auto writing(Mode) -> Writing override;
        auto makeChildPolicy() -> ChildPolicy override;

        void acceptWriting(Context::PatchRef, Mode);
        void acceptStewarding(Context::PatchRef, Rtid::Set dirtyTypes);
    };
}