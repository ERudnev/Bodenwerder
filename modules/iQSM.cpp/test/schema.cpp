#include <base/testing/macros.h>

#include <Atomic/model.q1.h>
#include <iQSM/schema.h>


namespace tests {
    void schema() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Model;

        auto schema = SchemaObject::assemble<Position>();

        EXPECT_EQ(schema.aspects.size(), 4);

        EXPECT_TRUE(schema.aspects.contains(Aspect<Molecule>::typeId));
        EXPECT_TRUE(schema.aspects.contains(Aspect<Atom>::typeId));
        EXPECT_TRUE(schema.aspects.contains(Aspect<Position>::typeId));

        EXPECT_EQ(schema.aspects.at(Aspect<Molecule>::typeId).invariants.structural.size(), Molecule::invariants.list.size());
        EXPECT_EQ(schema.aspects.at(Aspect<Atom>::typeId).invariants.structural.size(), Atom::invariants.list.size());
        EXPECT_EQ(schema.aspects.at(Aspect<Position>::typeId).invariants.structural.size(), Position::invariants.list.size());

        EXPECT_EQ(schema.aspects.at(Aspect<Molecule>::typeId).invariants.structural.size(), 0);
        EXPECT_EQ(schema.aspects.at(Aspect<Atom>::typeId).invariants.structural.size(), 1);
        EXPECT_EQ(schema.aspects.at(Aspect<Position>::typeId).invariants.structural.size(), 1);
    }
}
