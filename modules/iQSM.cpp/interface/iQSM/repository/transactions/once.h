#pragma once

#include <iQSM/delta.h>
#include <iQSM/repository/transaction.h>

namespace iqsm::repo {

    // Single-field transaction: one Writing in, one typed field-diff out on scope-exit.
    // Does not expose Reading/Writing conversions (private base): only ctor + typed field ops.
    template<meta::Aspect Meta>
    struct Once final : private Transaction {
    public:
        explicit Once(Writing writing) : Transaction(std::move(writing)) {}
        using Id = ::iqsm::Id<Meta>;
        using Item = ::iqsm::Item<Meta>;
        using Op = typename delta::FieldDiff<Meta>::Operation;

        ~Once() override { on_finish(); }

        void add(Id id, Item after) {
            accumulated.ops.insert_or_assign(std::move(id), Op{std::nullopt, std::move(after)});
        }

        void remove(Id id, Item before) {
            accumulated.ops.insert_or_assign(std::move(id), Op{std::move(before), std::nullopt});
        }

        void update(Id id, Item before, Item after) {
            accumulated.ops.insert_or_assign(std::move(id), Op{std::move(before), std::move(after)});
        }

    private:
        void on_finish() override;
        void absorb(internals::repo::Commit::Result result) override;

        delta::FieldDiff<Meta> accumulated{};
    };
}

namespace iqsm::repo {

    template<meta::Aspect Meta>
    inline void Once<Meta>::absorb(internals::repo::Commit::Result result) {
        if (result.delta->empty()) return;

        const auto it = result.delta->fields.find(types::aspectId<Meta>());
        if (it == result.delta->fields.end()) return;
        accumulated.absorb(*base::shared_ref_cast<delta::FieldDiff<Meta>>(it->second));
    }

    template<meta::Aspect Meta>
    inline void Once<Meta>::on_finish() {
        if (this->unwinding()) return;
        if (not this->head.upstream) return;
        if (accumulated.empty()) {
            this->disconnect();
            return;
        }

        auto out = base::make_shared<delta::Fields>();
        out->fields.emplace(types::aspectId<Meta>(), base::make_shared<delta::FieldDiff<Meta>>(std::move(accumulated)));
        this->head.upstream({{}, out});
        this->disconnect();
    }
}
