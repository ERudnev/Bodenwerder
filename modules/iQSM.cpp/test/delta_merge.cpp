#include "_common.h"

#include <Atomic/varph.q1.h>
#include <iQSM/internals/fields_mutable.h>

namespace {
    using namespace iqsm;
    using namespace Q1CORE::Example::Varph;

    Delta addCharge(Spark::Id id, integer value)
    {
        auto fd = base::make_shared<delta::FieldDiff<Charge>>();
        delta::FieldDiff<Charge>::Operation op{};
        op.after = Facet<Charge>::create(Charge::Quantum{value});
        fd->ops.emplace(id, std::move(op));

        auto wd = base::make_shared<delta::Fields>();
        wd->fields.emplace(Facet<Charge>::typeId, freeze(fd));
        return freeze(wd);
    }
    Delta setCharge(Spark::Id id, integer before_value, integer after_value)
    {
        auto fd = base::make_shared<delta::FieldDiff<Charge>>();
        delta::FieldDiff<Charge>::Operation op{};
        op.before = Facet<Charge>::create(Charge::Quantum{before_value});
        op.after = Facet<Charge>::create(Charge::Quantum{after_value});
        fd->ops.emplace(id, std::move(op));

        auto wd = base::make_shared<delta::Fields>();
        wd->fields.emplace(Facet<Charge>::typeId, freeze(fd));
        return freeze(wd);
    }
    Delta deleteCharge(Spark::Id id, integer before_value)
    {
        auto fd = base::make_shared<delta::FieldDiff<Charge>>();
        delta::FieldDiff<Charge>::Operation op{};
        op.before = Facet<Charge>::create(Charge::Quantum{before_value});
        fd->ops.emplace(id, std::move(op));

        auto wd = base::make_shared<delta::Fields>();
        wd->fields.emplace(Facet<Charge>::typeId, freeze(fd));
        return freeze(wd);
    }

    // slow comparison, good for testing only
    bool are_equal(Delta a, Delta b) {
        if (a == b) { return true; }

        if (a->fields.size() != b->fields.size()) { return false; }
        for (const auto& kv : a->fields) {
            if (not b->fields.contains(kv.first)) { return false; }
        }

        if (not a->fields.contains(Facet<Charge>::typeId)) { return false; }

        const auto au = a->fields.at(Facet<Charge>::typeId);
        const auto bu = b->fields.at(Facet<Charge>::typeId);

        const auto af = base::shared_ref_cast<const delta::FieldDiff<Charge>>(au);
        const auto bf = base::shared_ref_cast<const delta::FieldDiff<Charge>>(bu);

        auto item_equal = [](Facet<Charge>::Item x, Facet<Charge>::Item y) {
            if (x == y) { return true; }
            return x->value == y->value;
        };

        if (af->ops.size() != bf->ops.size()) { return false; }
        for (const auto& kv : af->ops) {
            const auto& id = kv.first;
            const auto& ao = kv.second;
            if (not bf->ops.contains(id)) { return false; }
            const auto& bo = bf->ops.at(id);

            if (ao.before.has_value() != bo.before.has_value()) { return false; }
            if (ao.before.has_value() and not item_equal(*ao.before, *bo.before)) { return false; }

            if (ao.after.has_value() != bo.after.has_value()) { return false; }
            if (ao.after.has_value() and not item_equal(*ao.after, *bo.after)) { return false; }
        }

        return true;
    }
}

namespace tests {
    void delta_merge() {
        const auto id = Spark::Id::generate_random();
        const auto other = Spark::Id::generate_random();

        const auto merged = [](Delta a, Delta b) -> Delta {
            iqsm::internals::FieldsMutable out{};
            out.absorb(std::move(a));
            out.absorb(std::move(b));
            return out.push();
        };

        EXPECT_TRUE(are_equal(merged(addCharge(id, integer{1}), addCharge(id, integer{2})), addCharge(id, integer{2})));
        EXPECT_TRUE(are_equal(merged(addCharge(id, integer{2}), addCharge(id, integer{1})), addCharge(id, integer{1})));
        EXPECT_TRUE(are_equal(merged(addCharge(id, integer{1}), addCharge(other, integer{2})), merged(addCharge(other, integer{2}), addCharge(id, integer{1}))));

        EXPECT_TRUE(are_equal(merged(setCharge(id, integer{1}, integer{2}), deleteCharge(id, integer{2})), deleteCharge(id, integer{2})));
        EXPECT_TRUE(are_equal(merged(deleteCharge(id, integer{1}), setCharge(id, integer{1}, integer{2})), deleteCharge(id, integer{1})));
        EXPECT_TRUE(are_equal(merged(deleteCharge(id, integer{1}), deleteCharge(id, integer{2})), deleteCharge(id, integer{1})));
        EXPECT_TRUE(are_equal(merged(setCharge(id, integer{1}, integer{2}), deleteCharge(other, integer{5})), merged(deleteCharge(other, integer{5}), setCharge(id, integer{1}, integer{2}))));

        // neutral element + idempotency
        EXPECT_TRUE(are_equal(merged(delta::empty(), deleteCharge(id, integer{1})), deleteCharge(id, integer{1})));
        EXPECT_TRUE(are_equal(merged(deleteCharge(id, integer{1}), delta::empty()), deleteCharge(id, integer{1})));
        EXPECT_TRUE(are_equal(merged(deleteCharge(id, integer{1}), deleteCharge(id, integer{2})), deleteCharge(id, integer{1})));

        // sum of sequential changes: (1->2) + (2->3) == (1->3)
        EXPECT_TRUE(are_equal(
            merged(setCharge(id, integer{1}, integer{2}), setCharge(id, integer{2}, integer{3})),
            setCharge(id, integer{1}, integer{3})));

        // tolerant sequential changes: (1->2) + (1->3) == (1->3)  ("who came later wins")
        EXPECT_TRUE(are_equal(
            merged(setCharge(id, integer{1}, integer{2}), setCharge(id, integer{1}, integer{3})),
            setCharge(id, integer{1}, integer{3})));
    }
}


