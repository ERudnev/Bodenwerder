#pragma once

#include <cstdlib>
#include <memory>
#include <optional>
#include <utility>

#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/contexts/operational.h>
#include <fQSM/processing/contexts/direct.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/contexts/review.h>
#include <fQSM/processing/transaction.h>
#include <fQSM/processing/orchestrators/branch.h>

namespace fqsm::processing::orchestrator {

    // Debug Realm: Gate collapse only parks the raw patch; normalize/integrate run in finish_patch().
    struct RealmSafe : Transaction {
        RealmSafe(Schema schema) : reality(schema) {}
        RealmSafe(const RealmSafe& other) : reality(static_cast<const State&>(other.reality)) {}
        RealmSafe(const State& other) : reality(other) {}

        template<typename F>
        auto branch(F&& worker) -> std::invoke_result_t<F, Writing> {
            Branch context(*this);
            return std::invoke(std::forward<F>(worker), static_cast<Writing>(context));
        }

        operator Reading() const override { return View(reality); }
        const model::complex::State* operator->() const { return &reality; }

        operator Stewarding();

        auto result() const -> const model::complex::Patch::Result& { return lastResult; }

        void finish_patch();

    private:
        struct DeferredWriting {
            std::shared_ptr<model::complex::Patch> patch;
            Mode mode = Mode::normal;
        };

        struct DeferredStewarding {
            std::shared_ptr<model::complex::Patch> patch;
            Rtid::Set tainted;
        };

        model::complex::Reality reality;
        model::complex::Patch::Result lastResult;
        std::optional<DeferredWriting> deferred_writing;
        std::optional<DeferredStewarding> deferred_stewarding;

        auto writing(Mode) -> Writing override;
        auto makeChildPolicy() -> ChildPolicy override;

        void acceptWriting(Context::PatchRef, Mode);
        void acceptStewarding(Context::PatchRef, Rtid::Set dirtyTypes);

        void crash_if_deferred() const;
    };
}
