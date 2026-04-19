#pragma once

#include <optional>
#include <utility>

#include <iQSM/internals/fields_mutable.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/meta/global.h>
#include <iQSM/repository/transaction.h>

// Staged batches typed field ops (add/remove/update) into FieldsMutable and emits one Delta on finish(); the world
// snapshot stays the read-only baseline until then.
// Same buffer-and-flush core as Accumulator; Staged favors many small direct add_op merges, Accumulator favors
// absorbing pre-built Deltas from child transactions.

namespace iqsm::repo {
    struct Staged final : Transaction {
        internals::FieldsMutable staged{};

        explicit Staged(Reading reading) : Transaction(reading) {}
        explicit Staged(Writing writing) : Transaction(std::move(writing)) {}
        explicit Staged(Transaction& parent) : Transaction(parent) {}
        ~Staged() override { finish(); }

        void finish() override;

        template<meta::Aspect Meta>
        void add(Id<Meta> id, Item<Meta> after) {
            using Op = typename delta::FieldDiff<Meta>::Operation;
            staged.add_op<Meta>(std::move(id), Op{std::nullopt, std::move(after)});
        }

        template<meta::Aspect Meta>
        void remove(Id<Meta> id, Item<Meta> before) {
            using Op = typename delta::FieldDiff<Meta>::Operation;
            staged.add_op<Meta>(std::move(id), Op{std::move(before), std::nullopt});
        }

        template<meta::Aspect Meta>
        void update(Id<Meta> id, Item<Meta> before, Item<Meta> after) {
            using Op = typename delta::FieldDiff<Meta>::Operation;
            staged.add_op<Meta>(std::move(id), Op{std::move(before), std::move(after)});
        }

        template<meta::Aspect Meta>
        void set_global(typename meta::Global<Meta> before, typename meta::Global<Meta> after) {
            staged.set_global<Meta>(std::move(before), std::move(after));
        }

    protected:
        void absorb(Delta delta) override {
            staged.absorb(head.state->schema, std::move(delta));
        }
    };

    inline void Staged::finish() {
        if (unwinding()) return;
        if (not head.upstream)
            return;
        head.upstream(staged.push());
        disconnect();
    }
}
