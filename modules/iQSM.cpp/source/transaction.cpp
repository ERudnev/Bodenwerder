#include <iQSM/operations/transaction.h>

#include <utility>

#include <iQSM/delta.h>
#include <iQSM/operations/integration.h>

namespace iqsm::ops {
    Transaction::Context::Context(Transaction& owner)
        : world(owner.world)
        , owner(&owner)
    {}

    void Transaction::Context::absorb(Delta update) { owner->absorb(update); }
    void Transaction::Context::integrate() { owner->integrate(); }
    void Transaction::Context::validate() { owner->validate(); }

    Transaction::Transaction(World&& world, bool auto_integrate)
        : world(std::move(world))
        , auto_integrate(auto_integrate)
        , inbox(::iqsm::delta::empty())
        , integrated(::iqsm::delta::empty())
    {}

    Transaction Transaction::accumulator(World world) { return Transaction{std::move(world), false}; }
    Transaction Transaction::integrator(World world) { return Transaction{std::move(world), true}; }

    Transaction::operator Context() { return Context{*this}; }
    Transaction::Context Transaction::context() { return Context{*this}; }

    void Transaction::absorb(Delta update) {
        inbox = merge(inbox, update);
        if (auto_integrate) integrate();
    }

    Delta Transaction::summary() const {
        return merge(integrated, inbox);
    }

    void Transaction::integrate() {
        if (inbox->empty()) return;
        world = ::iqsm::ops::integrate(world, inbox);
        integrated = merge(integrated, inbox);
        inbox = ::iqsm::delta::empty();
    }

    void Transaction::validate() {
        integrate();
        world = ::iqsm::ops::validate(world);
    }

}

