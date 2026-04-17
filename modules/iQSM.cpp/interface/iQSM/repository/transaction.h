#pragma once

#include <exception>
#include <utility>

#include <iQSM/delta.h>
#include <iQSM/repository/commit.h>
#include <iQSM/repository/permit.h>
#include <cstddef>


namespace iqsm::repo {

    // Interface, can consume Commit from Permit and keeps changes as local, making one final push/summary
    struct Transaction {
    protected:
        using Commit = internals::repo::Commit;
    public:
        Transaction(World world)
            : uncaught_exceptions_at_enter_(std::uncaught_exceptions())
            , root(world)
            , head(world, {})
        {}
        Transaction(Permit&& permit)
            : uncaught_exceptions_at_enter_(std::uncaught_exceptions())
            , root(permit.stolen.state)
            , head(std::move(permit.stolen))
        {}
        Transaction(Transaction& parent) : Transaction(static_cast<Writing>(parent)) {}

        Transaction(const Transaction&)=delete;
        Transaction(Transaction&&)=delete;
        virtual ~Transaction()=default;

        void submit(Delta delta) { absorb(delta); finish(); }
        virtual void finish() = 0; // store all updates to caller (if have caller)

        operator Reading() const { return head.state; }
        // Upstream is the single choke point for child→parent deltas: do not propagate empty packets.
        operator Writing() { return Permit{ Commit{ head.state, [this](Delta delta) { if (delta->empty()) return; this->absorb(std::move(delta)); } } }; }

        // Snapshot read of head.state (Commit baseline). Pending absorbs (e.g. Accumulator/Staged buffers)
        // are not merged into this view — by design for those transaction kinds.
        World operator*() const { return head.state; }
        auto operator->() const { return head.state.operator->(); }

    protected:
        friend struct Staged;

        // std::uncaught_exceptions() at ctor; unwinding() == true → unwind since then → finish() no-ops early.
        const int uncaught_exceptions_at_enter_{};

        bool unwinding() const noexcept { return std::uncaught_exceptions() > uncaught_exceptions_at_enter_; }

        World root; // this is state when Transaction was started. Like Git "base" (can be rebased)
        Commit head; // current transaction state as "commit": head and "how to send back" (Commit(w, null) is legal as "standalone")      

        void disconnect() { head.upstream = {}; }
        // each Transaction class must define own policy of receiving final change through head.upstream
        virtual void absorb(Delta delta) = 0;
    };
}
