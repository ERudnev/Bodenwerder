#include <base/testing/macros.h>

#include <Atomic/model.q1.h>
#include <iQSM/operations/integration.h>

namespace {
    using namespace iqsm;
    using namespace Q1CORE::Example::Model;

    Delta addMolecule(Molecule::Id id, std::string name)
    {
        auto fd = std::make_shared<delta::FieldDiff<Molecule>>();
        fd->added = fd->added.insert(id, Aspect<Molecule>::create({std::move(name)}));

        auto wd = std::make_shared<delta::WorldState>();
        wd->fields = wd->fields.insert(
            Aspect<Molecule>::typeId,
            std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));
        return freeze(wd);
    }
    Delta renameMolecule(Molecule::Id id, std::string oldname, std::string newname)
    {
        auto fd = std::make_shared<delta::FieldDiff<Molecule>>();
        fd->changed = fd->changed.insert(id, delta::FieldDiff<Molecule>::Change{
            .before = Aspect<Molecule>::create({std::move(oldname)}),
            .after = Aspect<Molecule>::create({std::move(newname)}),
        });

        auto wd = std::make_shared<delta::WorldState>();
        wd->fields = wd->fields.insert(
            Aspect<Molecule>::typeId,
            std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));
        return freeze(wd);
    }
    Delta deleteMolecule(Molecule::Id id)
    {
        auto fd = std::make_shared<delta::FieldDiff<Molecule>>();
        fd->deleted = fd->deleted.insert(id);

        auto wd = std::make_shared<delta::WorldState>();
        wd->fields = wd->fields.insert(
            Aspect<Molecule>::typeId,
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

        if (not a->fields.contains(Aspect<Molecule>::typeId)) { return false; }

        const auto au = a->fields.at(Aspect<Molecule>::typeId);
        const auto bu = b->fields.at(Aspect<Molecule>::typeId);

        auto af = std::dynamic_pointer_cast<const delta::FieldDiff<Molecule>>(au);
        auto bf = std::dynamic_pointer_cast<const delta::FieldDiff<Molecule>>(bu);
        if (not af || not bf) { return false; }

        auto item_equal = [](Aspect<Molecule>::Item x, Aspect<Molecule>::Item y) {
            if (x == y) { return true; }
            if (not x || not y) { return false; }
            return x->name == y->name;
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
        const auto id = Molecule::Id::generate_random();
        const auto other = Molecule::Id::generate_random();
        using namespace iqsm::ops;

        EXPECT_TRUE(are_equal(merge(addMolecule(id, "Water"), addMolecule(id, "Sugar")), addMolecule(id, "Water")));
        EXPECT_TRUE(are_equal(merge(addMolecule(id, "Sugar"), addMolecule(id, "Water")), addMolecule(id, "Sugar")));
        EXPECT_TRUE(are_equal(merge(addMolecule(id, "Water"), addMolecule(other, "Sugar")), merge(addMolecule(other, "Sugar"), addMolecule(id, "Water"))));

        EXPECT_TRUE(are_equal(merge(renameMolecule(id, "A", "B"), deleteMolecule(id)), deleteMolecule(id)));
        EXPECT_TRUE(are_equal(merge(deleteMolecule(id), renameMolecule(id, "A", "B")), deleteMolecule(id)));
        EXPECT_TRUE(are_equal(merge(deleteMolecule(id), deleteMolecule(id)), deleteMolecule(id)));
        EXPECT_TRUE(are_equal(merge(renameMolecule(id, "A", "B"), deleteMolecule(other)), merge(deleteMolecule(other), renameMolecule(id, "A", "B"))));

        // neutral element + idempotency
        EXPECT_TRUE(are_equal(merge(nullptr, deleteMolecule(id)), deleteMolecule(id)));
        EXPECT_TRUE(are_equal(merge(deleteMolecule(id), nullptr), deleteMolecule(id)));
        EXPECT_TRUE(are_equal(merge(deleteMolecule(id), deleteMolecule(id)), deleteMolecule(id)));

        // sum of sequential renames: (A->B) + (B->C) == (A->C)
        EXPECT_TRUE(are_equal(
            merge(renameMolecule(id, "A", "B"), renameMolecule(id, "B", "C")),
            renameMolecule(id, "A", "C")));

        // tolerant sequential renames: (A->B) + (A->C) == (A->C)  ("who came later wins")
        EXPECT_TRUE(are_equal(
            merge(renameMolecule(id, "A", "B"), renameMolecule(id, "A", "C")),
            renameMolecule(id, "A", "C")));
    }
}


