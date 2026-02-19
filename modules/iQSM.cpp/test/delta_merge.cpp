#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>

namespace {
    using namespace iqsm;
    using namespace Q1CORE::Example::Varph;

    Delta addElectron(Spark::Id spark, Electron::Id id, std::string legend)
    {
        auto fd = std::make_shared<delta::FieldDiff<Electron>>();
        fd->added = fd->added.insert(id, Aspect<Electron>::create({spark, std::move(legend)}));

        auto wd = std::make_shared<delta::WorldState>();
        wd->fields = wd->fields.insert(
            Aspect<Electron>::typeId,
            std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));
        return freeze(wd);
    }
    Delta renameElectron(Spark::Id spark, Electron::Id id, std::string oldlegend, std::string newlegend)
    {
        auto fd = std::make_shared<delta::FieldDiff<Electron>>();
        fd->changed = fd->changed.insert(id, delta::FieldDiff<Electron>::Change{
            .before = Aspect<Electron>::create({spark, std::move(oldlegend)}),
            .after = Aspect<Electron>::create({spark, std::move(newlegend)}),
        });

        auto wd = std::make_shared<delta::WorldState>();
        wd->fields = wd->fields.insert(
            Aspect<Electron>::typeId,
            std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));
        return freeze(wd);
    }
    Delta deleteElectron(Electron::Id id)
    {
        auto fd = std::make_shared<delta::FieldDiff<Electron>>();
        fd->deleted = fd->deleted.insert(id);

        auto wd = std::make_shared<delta::WorldState>();
        wd->fields = wd->fields.insert(
            Aspect<Electron>::typeId,
            std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));
        return freeze(wd);
    }

    // slow comparison, good for testing only
    bool are_equal(Delta a, Delta b) {
        if (a == b) { return true; }
        if (not a || not b) { return false; }

        if (a->fields.size() != b->fields.size()) { return false; }
        for (const auto& kv : a->fields) {
            if (not b->fields.contains(kv.first)) { return false; }
        }

        if (not a->fields.contains(Aspect<Electron>::typeId)) { return false; }

        const auto au = a->fields.at(Aspect<Electron>::typeId);
        const auto bu = b->fields.at(Aspect<Electron>::typeId);

        auto af = std::dynamic_pointer_cast<const delta::FieldDiff<Electron>>(au);
        auto bf = std::dynamic_pointer_cast<const delta::FieldDiff<Electron>>(bu);
        if (not af || not bf) { return false; }

        auto item_equal = [](Aspect<Electron>::Item x, Aspect<Electron>::Item y) {
            if (x == y) { return true; }
            if (not x || not y) { return false; }
            return x->spark == y->spark && x->legend == y->legend;
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
        const auto spark = Spark::Id::generate_random();
        const auto id = Electron::Id::generate_random();
        const auto other = Electron::Id::generate_random();
        using namespace iqsm::ops;

        EXPECT_TRUE(are_equal(merge(addElectron(spark, id, "1s"), addElectron(spark, id, "2s")), addElectron(spark, id, "1s")));
        EXPECT_TRUE(are_equal(merge(addElectron(spark, id, "2s"), addElectron(spark, id, "1s")), addElectron(spark, id, "2s")));
        EXPECT_TRUE(are_equal(merge(addElectron(spark, id, "1s"), addElectron(spark, other, "2s")), merge(addElectron(spark, other, "2s"), addElectron(spark, id, "1s"))));

        EXPECT_TRUE(are_equal(merge(renameElectron(spark, id, "A", "B"), deleteElectron(id)), deleteElectron(id)));
        EXPECT_TRUE(are_equal(merge(deleteElectron(id), renameElectron(spark, id, "A", "B")), deleteElectron(id)));
        EXPECT_TRUE(are_equal(merge(deleteElectron(id), deleteElectron(id)), deleteElectron(id)));
        EXPECT_TRUE(are_equal(merge(renameElectron(spark, id, "A", "B"), deleteElectron(other)), merge(deleteElectron(other), renameElectron(spark, id, "A", "B"))));

        // neutral element + idempotency
        EXPECT_TRUE(are_equal(merge(nullptr, deleteElectron(id)), deleteElectron(id)));
        EXPECT_TRUE(are_equal(merge(deleteElectron(id), nullptr), deleteElectron(id)));
        EXPECT_TRUE(are_equal(merge(deleteElectron(id), deleteElectron(id)), deleteElectron(id)));

        // sum of sequential renames: (A->B) + (B->C) == (A->C)
        EXPECT_TRUE(are_equal(
            merge(renameElectron(spark, id, "A", "B"), renameElectron(spark, id, "B", "C")),
            renameElectron(spark, id, "A", "C")));

        // tolerant sequential renames: (A->B) + (A->C) == (A->C)  ("who came later wins")
        EXPECT_TRUE(are_equal(
            merge(renameElectron(spark, id, "A", "B"), renameElectron(spark, id, "A", "C")),
            renameElectron(spark, id, "A", "C")));
    }
}


