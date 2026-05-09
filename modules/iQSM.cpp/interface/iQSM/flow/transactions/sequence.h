#pragma once

#include <iQSM/operations/integration.h>
#include <iQSM/repository/transaction.h>
#include <iQSM/internals/fields_mutable.h>


namespace iqsm::flow {

    // Mirrors iqsm::flow::Sequence: integrate + FieldsMutable (no validate_smart on absorb);
    // upstream receives accumulated delta on on_finish (see interface/iQSM/repository/transactions/sequence.h).
    struct Sequence : Transaction {
        explicit Sequence(Reading reading) : Transaction(reading) {}
        explicit Sequence(Permit writing) : Transaction(std::move(writing)) {}
        explicit Sequence(Transaction& parent) : Transaction(parent) {}
        ~Sequence() override;

        Delta delta();
        Delta push();

    protected:
        void on_finish() override;        
        void absorb(Channel::Result result) override;

    private:
        internals::FieldsMutable accumulated{};
    };
}

// impl:
namespace iqsm::flow {

    inline Sequence::~Sequence() {
        on_finish();
    }

    inline Delta Sequence::delta() {
        return accumulated.delta();
    }

    inline Delta Sequence::push() {
        return accumulated.push();
    }

    inline void Sequence::absorb(Channel::Result result) {
        if (result.maybeState.exists()) {
            head.state = result.maybeState;
        } else {
            head.state = operations::integrate(head.state, result.delta);
        }
        accumulated.absorb(head.state->schema, std::move(result.delta));
    }

    inline void Sequence::on_finish() {
        if (unwinding()) return;
        if (not head.upstream)
            return;
        head.upstream({head.state, push()});
        disconnect();
    }
}
