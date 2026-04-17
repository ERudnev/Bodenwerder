#pragma once

#include "transaction.h"

namespace iqsm_mock::repo {

    // Mirrors iqsm::repo::Sequence: integrate + FieldsMutable (no validate_smart on absorb);
    // upstream receives accumulated delta on finish (see interface/iQSM/repository/sequence.h).
    struct Sequence : Transaction {
        explicit Sequence(Writing writing) : Transaction(std::move(writing)) {}
        ~Sequence() override;

        Delta delta() const;
        Delta push();

    protected:
        void absorb(Delta delta) override;
        void finish() override;

    private:
        internals::FieldsMutable accumulated{};
    };
}

// impl:
namespace iqsm_mock::repo {

    Sequence::~Sequence() {
        base::message("-sequence");
        finish();
    }

    Delta Sequence::delta() const {
        return accumulated.snapshot(head.state->schema);
    }

    Delta Sequence::push() {
        return accumulated.push();
    }

    void Sequence::absorb(Delta delta) {
        head.state = operations::integrate(head.state, delta);
        accumulated.absorb(head.state->schema, std::move(delta));
    }

    void Sequence::finish() {
        if (not head.upstream)
            return;
        head.upstream(push());
        disconnect();
    }
}
