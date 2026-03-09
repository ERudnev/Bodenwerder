#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>

namespace {
    using namespace iqsm;
    using namespace Q1CORE::Example::Varph;

    Delta addCharge(Spark::Id id, integer value)
    {
        auto fd = base::make_shared<delta::FieldDiff<Charge>>();
        delta::FieldDiff<Charge>::Operation op{};
        op.add = Facet<Charge>::create(Charge::Quantum{value});
        fd->ops = fd->ops.insert(id, std::move(op));

        auto wd = base::make_shared<delta::Fields>();
        wd->fields = wd->fields.insert(
            Facet<Charge>::typeId,
            freeze(fd));
        return freeze(wd);
    }
    Delta setCharge(Spark::Id id, integer before_value, integer after_value)
    {
        auto fd = base::make_shared<delta::FieldDiff<Charge>>();
        delta::FieldDiff<Charge>::Operation op{};
        op.change = std::pair<Facet<Charge>::Item, Facet<Charge>::Item>{
            Facet<Charge>::create(Charge::Quantum{before_value}),
            Facet<Charge>::create(Charge::Quantum{after_value}),
        };
        fd->ops = fd->ops.insert(id, std::move(op));

        auto wd = base::make_shared<delta::Fields>();
        wd->fields = wd->fields.insert(
            Facet<Charge>::typeId,
            freeze(fd));
        return freeze(wd);
    }
    Delta deleteCharge(Spark::Id id)
    {
        auto fd = base::make_shared<delta::FieldDiff<Charge>>();
        delta::FieldDiff<Charge>::Operation op{};
        op.remove = true;
        fd->ops = fd->ops.insert(id, std::move(op));

        auto wd = base::make_shared<delta::Fields>();
        wd->fields = wd->fields.insert(
            Facet<Charge>::typeId,
            freeze(fd));
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

            if (ao.remove != bo.remove) { return false; }

            if (ao.add.has_value() != bo.add.has_value()) { return false; }
            if (ao.add.has_value() and not item_equal(*ao.add, *bo.add)) { return false; }

            if (ao.change.has_value() != bo.change.has_value()) { return false; }
            if (ao.change.has_value()) {
                if (not item_equal(ao.change->first, bo.change->first)) { return false; }
                if (not item_equal(ao.change->second, bo.change->second)) { return false; }
            }
        }

        return true;
    }
}

namespace tests {
    void delta_merge() {
        const auto id = Spark::Id::generate_random();
        const auto other = Spark::Id::generate_random();
        using namespace iqsm::ops;

        EXPECT_TRUE(are_equal(merge(addCharge(id, integer{1}), addCharge(id, integer{2})), addCharge(id, integer{1})));
        EXPECT_TRUE(are_equal(merge(addCharge(id, integer{2}), addCharge(id, integer{1})), addCharge(id, integer{2})));
        EXPECT_TRUE(are_equal(merge(addCharge(id, integer{1}), addCharge(other, integer{2})), merge(addCharge(other, integer{2}), addCharge(id, integer{1}))));

        EXPECT_TRUE(are_equal(merge(setCharge(id, integer{1}, integer{2}), deleteCharge(id)), deleteCharge(id)));
        EXPECT_TRUE(are_equal(merge(deleteCharge(id), setCharge(id, integer{1}, integer{2})), deleteCharge(id)));
        EXPECT_TRUE(are_equal(merge(deleteCharge(id), deleteCharge(id)), deleteCharge(id)));
        EXPECT_TRUE(are_equal(merge(setCharge(id, integer{1}, integer{2}), deleteCharge(other)), merge(deleteCharge(other), setCharge(id, integer{1}, integer{2}))));

        // neutral element + idempotency
        EXPECT_TRUE(are_equal(merge(delta::empty(), deleteCharge(id)), deleteCharge(id)));
        EXPECT_TRUE(are_equal(merge(deleteCharge(id), delta::empty()), deleteCharge(id)));
        EXPECT_TRUE(are_equal(merge(deleteCharge(id), deleteCharge(id)), deleteCharge(id)));

        // sum of sequential changes: (1->2) + (2->3) == (1->3)
        EXPECT_TRUE(are_equal(
            merge(setCharge(id, integer{1}, integer{2}), setCharge(id, integer{2}, integer{3})),
            setCharge(id, integer{1}, integer{3})));

        // tolerant sequential changes: (1->2) + (1->3) == (1->3)  ("who came later wins")
        EXPECT_TRUE(are_equal(
            merge(setCharge(id, integer{1}, integer{2}), setCharge(id, integer{1}, integer{3})),
            setCharge(id, integer{1}, integer{3})));
    }
}


