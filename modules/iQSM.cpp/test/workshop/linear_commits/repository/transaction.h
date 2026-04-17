#pragma once

#include "commit.h"
#include "permit.h"
#include <cstddef>

namespace iqsm_mock::repo {

    struct Staged;

    // Interface, can consume Commit from Permit and keeps changes as local, making one final push/summary
    struct Transaction {
        Transaction(World world) : root(world), head(world, {}) {}
        Transaction(Permit permit) : root(permit.stolen.state), head(std::move(permit.stolen)) {}

        Transaction(const Transaction&)=delete;
        Transaction(Transaction&&)=delete;
        virtual ~Transaction()=default;

        operator Reading() const { return head.state; }
        operator Writing() { return Permit{ Commit{head.state, [this](Delta delta) { this->absorb(std::move(delta)); }}};}

        // store all updates to caller (if have caller)
        virtual void finish() = 0;
        // experimental:
        //void rebase(World newState) { head.state = newState; }
    protected:
        friend struct Staged;

        // Staged scope exit: one baked delta into the hidden Commit pipe (see repository/staged.h).
        void flush_staged_delta(Delta d) { head.receive(std::move(d)); }

        World root; // this is state when Transaction was started. Like Git "base" (can be rebased)
        Commit head; // current transaction state as "commit": head and "how to send back" (Commit(w, null) is legal as "standalone")      

        void disconnect() { head.upstream = {}; }
        // each Transaction class must define own policy of receiving changes through head.upstream
        virtual void absorb(Delta delta) = 0;
    };
}

// impl:
namespace iqsm_mock::repo {

    /*
    void Transaction::merge(Transaction& other) {
        base::message("merge...");
        // fast-forward is possible ?
        if (other.root == head.state)
            head.state = other.head.state;
        // long way: evaluate own delta , other Dalta and merge:
        const auto myDelta = operations::make_delta(other.
    }
    */
}