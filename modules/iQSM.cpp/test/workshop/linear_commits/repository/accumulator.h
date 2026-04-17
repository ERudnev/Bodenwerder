#pragma once

#include "transaction.h"

namespace iqsm_mock::repo {

    // Процессный слой над скрытым Commit: толстая дельта копится отдельно, снимок мира для чтения не двигаем.
    // Смысл как у iqsm::repo::Accumulator (буфер без integrate/validate в World), но язык — Transaction / Permit / finish.
    struct Accumulator : Transaction {
        explicit Accumulator(Writing writing) : Transaction(std::move(writing)) {}
        ~Accumulator() override;

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

    Accumulator::~Accumulator() {
        base::message("-accumulator");
        finish();
    }

    Delta Accumulator::delta() const {
        return accumulated.snapshot(head.state->schema);
    }

    Delta Accumulator::push() {
        return accumulated.push();
    }

    void Accumulator::absorb(Delta delta) {
        accumulated.absorb(head.state->schema, std::move(delta));
    }

    void Accumulator::finish() {
        if (not head.upstream)
            return;
        head.upstream(push());
        disconnect();
    }
}
