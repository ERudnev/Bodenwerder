#pragma once

#include <iQSM/meta/aspect.h>
#include <iQSM/delta.h>
#include <iQSM/repository/transaction.h>

namespace iqsm::repo {

    // Single-field transaction: one Writing in, one typed field-diff out on scope-exit.
    // Does not expose Reading/Writing conversions (private base): only ctor + typed field ops.
    template<aspect::Any Meta>
    struct Elementary final : private Transaction {
    public:
        explicit Elementary(Writing writing) : Transaction(std::move(writing)) {}
        using Id = ::iqsm::Id<Meta>;
        using Quantum = ::iqsm::Quantum<Meta>;
        using Element = typename Meta::Runtime::Element::State;
        using Versioning = typename Meta::Runtime::Versioning;
        using Differential = meta::state::Differentiation<Meta, Versioning::value>;
        using Patch = typename Meta::Runtime::Element::Patch;

        ~Elementary() override { on_finish(); }

        void add(Id id, Element after) {
            accumulated.ops.insert_or_assign(id, Differential::add(std::move(after)));
        }
        
        void remove(Id id, Element before) {
            accumulated.ops.insert_or_assign(id, Differential::remove(std::move(before)));
        }
        
        void change(Id id, Element before, Element after) {
            accumulated.ops.insert_or_assign(id, Differential::change(std::move(before), std::move(after)));
        }

    private:
        void on_finish() override;
        void absorb(internals::repo::Commit::Result result) override;

        delta::FieldDiff<Meta> accumulated{};
    };
}

namespace iqsm::repo {

    template<aspect::Any Meta>
    inline void Elementary<Meta>::absorb(internals::repo::Commit::Result result) {
        if (result.delta->empty()) return;

        const auto it = result.delta->fields.find(types::aspectId<Meta>());
        if (it == result.delta->fields.end()) return;
        accumulated.absorb(*base::shared_ref_cast<delta::FieldDiff<Meta>>(it->second));
    }

    template<aspect::Any Meta>
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
