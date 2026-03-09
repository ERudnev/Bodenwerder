#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>

namespace {
    using namespace iqsm;
    using namespace Q1CORE::Example::Varph;

    Delta addCharge(Spark::Id id, integer value)
    {
        auto fd = base::make_shared<delta::FieldDiff<Charge>>();
        fd->added = fd->added.insert(id, Facet<Charge>::create(Charge::Quantum{value}));

        auto wd = base::make_shared<delta::Fields>();
        wd->fields = wd->fields.insert(
            Facet<Charge>::typeId,
            freeze(fd));
        return freeze(wd);
    }
    Delta setCharge(Spark::Id id, integer before_value, integer after_value)
    {
        auto fd = base::make_shared<delta::FieldDiff<Charge>>();
        fd->changed = fd->changed.insert(id, delta::FieldDiff<Charge>::Change{
            .before = Facet<Charge>::create(Charge::Quantum{before_value}),
            .after = Facet<Charge>::create(Charge::Quantum{after_value}),
        });

        auto wd = base::make_shared<delta::Fields>();
        wd->fields = wd->fields.insert(
            Facet<Charge>::typeId,
            freeze(fd));
        return freeze(wd);
    }
    Delta deleteCharge(Spark::Id id)
    {
        auto fd = base::make_shared<delta::FieldDiff<Charge>>();
        fd->deleted = fd->deleted.insert(id);

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

        if (af->added.size() != bf->added.size()) { return false; }
        for (const auto& kv : af->added) {
            if (not bf->added.contains(kv.first)) { return false; }
            if (not item_equal(kv.second, bf->added.at(kv.first))) { return false; }
        }

        if (af->deleted.size() != bf->deleted.size()) { return false; }
        for (const auto& id : af->deleted) {
            if (not bf->deleted.contains(id)) { return false; }
        }

        if (af->changed.size() != bf->changed.size()) { return false; }
        for (const auto& kv : af->changed) {
            if (not bf->changed.contains(kv.first)) { return false; }
            const auto ac = kv.second;
            const auto bc = bf->changed.at(kv.first);
            if (not item_equal(ac.before, bc.before)) { return false; }
            if (not item_equal(ac.after, bc.after)) { return false; }
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


