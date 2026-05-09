#pragma once

#include <exception>
#include <stdexcept>
#include <utility>

#include <iQSM/delta.h>
#include <iQSM/repository/channel.h>
#include <iQSM/repository/permit.h>
#include <cstddef>


namespace iqsm::flow {

    // Interface, can consume Channel from Permit and keeps changes as local, making one final push/summary
    struct Transaction {
    protected:
        using Channel = internals::flow::Channel;
    public:
        Transaction(Reading world)
            : uncaught_exceptions_at_enter(std::uncaught_exceptions())
            , root(world)
            , head(world, {})
        {}
        Transaction(Permit&& permit)
            : uncaught_exceptions_at_enter(std::uncaught_exceptions())
            , root(permit.stolen.state)
            , head(std::move(permit.stolen))
        {}
        Transaction(Transaction& parent) : Transaction(static_cast<Writing>(parent)) {}

        Transaction(const Transaction&)=delete;
        Transaction(Transaction&&)=delete;
        virtual ~Transaction()=default;

        void submit(Delta delta) { absorb({{}, delta}); on_finish(); }
        // После on_finish() обнуляет root/head (мир и channel): повторный on_finish() тихо выходит по not head.upstream / пустому state.
        void complete() {
            on_finish();
            root.kill();
            head.kill();
        }

        operator Reading() const {
            if (not root.get()) throw std::logic_error("flow::Transaction: invalid (e.g. after complete())");
            return head.state;
        }
        // Upstream is the single choke point for child→parent deltas: do not propagate empty packets.
        operator Writing() {
            if (not root.get()) throw std::logic_error("flow::Transaction: invalid (e.g. after complete())");
            return Permit{ Channel{ head.state, [this](Channel::Result result) {
                if (not result.maybeState.exists() && result.delta->empty()) return;
                this->absorb(std::move(result));
            } } };
        }

        // Snapshot read of head.state (Channel baseline). Pending absorbs (e.g. Accumulator/Staged buffers)
        // are not merged into this view — by design for those transaction kinds.
        Reading operator*() const { return head.state; }
        auto operator->() const { return head.state.operator->(); }

    protected:
        friend struct Staged;

        // std::uncaught_exceptions() at ctor; unwinding() == true → unwind since then → on_finish() no-ops early.
        const int uncaught_exceptions_at_enter{};

        Reading root; // this is state when Transaction was started. Like Git "base" (can be rebased)
        Channel head; // current transaction state as "channel": head and "how to send back" (Channel(w, null) is legal as "standalone")      

        bool unwinding() const noexcept { return std::uncaught_exceptions() > uncaught_exceptions_at_enter; }
        void disconnect() { head.upstream = {}; }
        /// Like `operator Writing()`, but custom upstream instead of `absorb` (e.g. integrate-only for nested validation).
        Writing permit_with_upstream(Channel::Upstream sink) {
            return Permit{ Channel{ head.state, std::move(sink) } };
        }
        // each Transaction class must define own policy of receiving final change through head.upstream
        virtual void absorb(Channel::Result result) = 0;
        virtual void on_finish() = 0; // store all updates to caller (if have caller)
    };
}
