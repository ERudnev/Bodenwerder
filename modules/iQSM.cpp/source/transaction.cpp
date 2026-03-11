#include <iQSM/operations/transaction.h>

#include <utility>

#include <iQSM/delta.h>
#include <iQSM/operations/integration.h>

namespace iqsm::ops {
    Transaction::Transaction(World&& initial, Policy policy)
        : current(std::move(initial))
        , summary(::iqsm::delta::empty())
        , policy(policy)
    {}

    Transaction Transaction::accumulator(World initial) { return Transaction{std::move(initial), Policy::accumulator}; }
    Transaction Transaction::integrator(World initial) { return Transaction{std::move(initial), Policy::integrator}; }
    Transaction Transaction::validator(World initial) { return Transaction{std::move(initial), Policy::validator}; }

    static Transaction intercept_as(Transaction::Policy policy, World initial, Transaction&& from) {
        auto out = Transaction{std::move(initial), policy};
        out.summary = std::move(from.summary);
        out.dirty = from.dirty;
        return out;
    }

    Transaction Transaction::accumulator(World initial, Transaction&& from) { return intercept_as(Policy::accumulator, std::move(initial), std::move(from)); }
    Transaction Transaction::integrator(World initial, Transaction&& from) { return intercept_as(Policy::integrator, std::move(initial), std::move(from)); }
    Transaction Transaction::validator(World initial, Transaction&& from) { return intercept_as(Policy::validator, std::move(initial), std::move(from)); }

    void Transaction::absorb(Delta update) {
        summary = merge(summary, update);

        if (policy == Policy::accumulator) {
            dirty = true;
            return;
        }

        current = apply(std::move(current), update);
        dirty = false;
    }

    void Transaction::flush() {
        if (not dirty) return;
        if (summary->empty()) { dirty = false; return; }
        current = apply(std::move(current), summary);
        dirty = false;
    }

    void Transaction::validate() {
        if (dirty) flush();
        current = ::iqsm::ops::validate(std::move(current));
    }

    World Transaction::apply(World world, Delta update) const {
        if (policy == Policy::validator) {
            return integrate(std::move(world), std::move(update));
        }
        return integrate_raw(std::move(world), std::move(update));
    }
}

