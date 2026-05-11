#pragma once

#include <iQSM/delta.h>
#include <iQSM/repository/transaction.h>

namespace iqsm::repo {

    // Single-field transaction: one Writing in, one typed field-diff out on scope-exit.
    // Does not expose Reading/Writing conversions (private base): only ctor + typed field ops.
    template<meta::Aspect Meta>
    struct Elementary final : private Transaction {
    public:
        explicit Elementary(Writing writing) : Transaction(std::move(writing)) {}
        using Id = ::iqsm::Id<Meta>;
        using Node = ::iqsm::Node<Meta>;
        using Op = state::Chunk<Meta, state::policy::order::patch>;

        ~Elementary() override { on_finish(); }

        //void add(Id id, 


        /*
        void add(Id id, Node after) {
            accumulated.ops.insert_or_assign(std::move(id), Op{std::nullopt, std::move(after)});
        }

        void remove(Id id, Node before) {
            accumulated.ops.insert_or_assign(std::move(id), Op{std::move(before), std::nullopt});
        }

        void update(Id id, Node before, Node after) {
            accumulated.ops.insert_or_assign(std::move(id), Op{std::move(before), std::move(after)});
        }
        */

    private:
        void on_finish() override;
        void absorb(internals::repo::Commit::Result result) override;

        delta::FieldDiff<Meta> accumulated{};
    };
}

namespace iqsm::repo {

    template<meta::Aspect Meta>
    inline void Elementary<Meta>::absorb(internals::repo::Commit::Result result) {
        if (result.delta->empty()) return;

        const auto it = result.delta->fields.find(types::aspectId<Meta>());
        if (it == result.delta->fields.end()) return;
        accumulated.absorb(*base::shared_ref_cast<delta::FieldDiff<Meta>>(it->second));
    }

    template<meta::Aspect Meta>
    inline void Elementary<Meta>::on_finish() {
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
