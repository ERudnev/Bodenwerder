#pragma once

#include <iQSM/internals/fields_mutable.h>
#include <iQSM/repository/transaction.h>

namespace iqsm::repo {

    // Single-shot transaction: one Writing in, one submit(Delta) or scope-exit flush.
    // Does not expose Reading/Writing conversions (private base): only ctor + submit.
    struct Once final : private Transaction {
    public:
        explicit Once(Writing writing) : Transaction(std::move(writing)) {}

        using Transaction::submit;
        void finish() override;

        ~Once() override { finish(); }

    private:
        void absorb(Delta delta) override;

        internals::FieldsMutable accumulated{};
    };
}

namespace iqsm::repo {

    inline void Once::absorb(Delta delta) {
        accumulated.absorb(head.state->schema, std::move(delta));
    }

    inline void Once::finish() {
        if (unwinding()) return;
        if (not head.upstream) return;
        head.upstream(accumulated.push());
        disconnect();
    }
}
