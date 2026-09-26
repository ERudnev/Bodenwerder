#pragma once

#include <cassert>
#include <memory>
#include <utility>
#include <vector>

#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/transaction.h>
#include <fQSM/processing/orchestrators/branch.h>

namespace fqsm::processing::orchestrator {

    // Owns the reality and the sessions opened on it. A session is accepted (normalized, then integrated)
    // when its last handle ends: an unnamed Writing at the end of the full expression, a named one at its scope end.
    struct Realm : Transaction, private SessionOwner {
        Realm(Schema schema) : reality(complete(std::move(schema))) { assembleGlobals(); }
        Realm(const Realm& other) : Transaction(), SessionOwner(), reality(static_cast<const State&>(other.reality)) {}
        Realm(const State& other) : reality(other) {}
        ~Realm() override { assert(open.empty() and "fQSM: a session of this Realm is still open"); }

        // runs worker in a Branch; the Branch merges into this Realm when worker returns,
        // so the result must not refer to the session of the Branch
        template<typename Worker>
            requires (not SessionBound<std::invoke_result_t<Worker, Writing>>)
        auto branch(Worker&& worker) -> std::invoke_result_t<Worker, Writing> {
            Branch context(*this);
            return std::invoke(std::forward<Worker>(worker), static_cast<Writing>(context));
        }

        operator Reading() const override { return Reading(reality); }
        const model::complex::State* operator->() const { return &reality; }
        operator Stewarding();

        auto result() const -> const model::complex::Patch::Summary& { return lastResult; }

    private:
        model::complex::Reality reality;
        model::complex::Patch::Summary lastResult;
        std::vector<std::unique_ptr<Session>> open;

        auto open_session(bool silent, bool direct) -> Session&;
        auto writing(Mode) -> Writing override;
        auto child_base() const -> const model::complex::State& override { return reality; }
        void accept_child(ref<model::complex::Patch> patch) override { accept(patch, {}, false); }
        void release(Session&) override;

        static auto complete(Schema schema) -> Schema { schema->requireHosts(); return schema; }
        void assembleGlobals();
        void accept(ref<model::complex::Patch>, Rtid::Set tainted, bool silent);
    };
}
