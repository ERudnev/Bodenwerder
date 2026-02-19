#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>

#include "utilities/utilities.h"

namespace tests {
    void schema() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        { // assemble from one non-root:
            auto schema = SchemaObject::assemble<Chemical>();

            EXPECT_EQ(schema.aspects.size(), 3);

            EXPECT_TRUE(schema.aspects.contains(Aspect<Atom>::typeId));
            EXPECT_TRUE(schema.aspects.contains(Aspect<Spark>::typeId));
            EXPECT_EQ(utilities::type_names(schema),
                "{Q1CORE::Example::Varph::Atom, Q1CORE::Example::Varph::Chemical, Q1CORE::Example::Varph::Spark}");

            EXPECT_EQ(schema.aspects.at(Aspect<Atom>::typeId).invariants.structural.size(), Atom::invariants.list.size());
            EXPECT_EQ(schema.aspects.at(Aspect<Spark>::typeId).invariants.structural.size(), Spark::invariants.list.size());

            EXPECT_EQ(schema.aspects.at(Aspect<Atom>::typeId).invariants.structural.size(), 1);
            EXPECT_EQ(schema.aspects.at(Aspect<Spark>::typeId).invariants.structural.size(), 0);
            EXPECT_EQ(schema.aspects.at(Aspect<Chemical>::typeId).invariants.structural.size(), 1);
        }
        { // merge two Schema's
            const auto a = ops::schema::assemble<Atom>();
            const auto b = ops::schema::assemble<Chemical>();

            const auto ab = SchemaObject::merge(a, b);
            const auto ba = SchemaObject::merge(b, a);

            EXPECT_TRUE(ab != nullptr);
            EXPECT_TRUE(ba != nullptr);

            EXPECT_EQ(ab->aspects.size(), 3);
            EXPECT_EQ(ba->aspects.size(), 3);

            EXPECT_TRUE(ab->aspects.contains(Aspect<Spark>::typeId));
            EXPECT_TRUE(ab->aspects.contains(Aspect<Atom>::typeId));
            EXPECT_TRUE(ab->aspects.contains(Aspect<Chemical>::typeId));

            EXPECT_EQ(ab->aspects.at(Aspect<Atom>::typeId).invariants.structural.size(), 2);
            EXPECT_EQ(ba->aspects.at(Aspect<Atom>::typeId).invariants.structural.size(), 2);

            EXPECT_TRUE(ab->aspects.at(Aspect<Spark>::typeId).required_by.contains(Aspect<Atom>::typeId));
            EXPECT_TRUE(ab->aspects.at(Aspect<Atom>::typeId).required_by.contains(Aspect<Chemical>::typeId));
        }
    }
}
