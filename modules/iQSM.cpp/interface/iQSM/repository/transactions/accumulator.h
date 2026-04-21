#pragma once

#include <iQSM/internals/fields_mutable.h>
#include <iQSM/repository/transaction.h>

namespace iqsm::repo {

    // Процессный слой над скрытым Commit: толстая дельта копится отдельно, снимок мира для чтения не двигаем.
    // Смысл как у iqsm::repo::Accumulator (буфер без integrate/validate в World), но язык — Transaction / Permit / on_finish.
    struct Accumulator : Transaction {
        explicit Accumulator(Writing writing) : Transaction(std::move(writing)) {}
        explicit Accumulator(Transaction& parent) : Transaction(parent) {}
        ~Accumulator() override { on_finish(); }

        Delta delta() const;
        Delta push();

    protected:
        void on_finish() override;
        void absorb(Commit::Result result) override;

    private:
        internals::FieldsMutable accumulated{};
    };
}

// impl:
namespace iqsm::repo {

    inline Delta Accumulator::delta() const {
        return accumulated.snapshot(head.state->schema);
    }

    inline Delta Accumulator::push() {
        return accumulated.push();
    }

    inline void Accumulator::absorb(Commit::Result result) {
        accumulated.absorb(head.state->schema, std::move(result.delta));
    }

    inline void Accumulator::on_finish() {
        if (unwinding()) return;
        if (not head.upstream)
            return;
        head.upstream({{}, push()});
        disconnect();
    }
}
