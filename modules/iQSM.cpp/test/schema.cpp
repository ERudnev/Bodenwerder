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

            EXPECT_EQ(schema.aspects.size(), 5);

            EXPECT_TRUE(schema.aspects.contains(Facet<Atom>::typeId));
            EXPECT_TRUE(schema.aspects.contains(Facet<Spark>::typeId));
            EXPECT_TRUE(schema.aspects.contains(Facet<Charge>::typeId));
            EXPECT_TRUE(schema.aspects.contains(Facet<Electron>::typeId));
            EXPECT_EQ(utilities::type_names(schema),
                "{Q1CORE::Example::Varph::Atom, Q1CORE::Example::Varph::Charge, Q1CORE::Example::Varph::Chemical, Q1CORE::Example::Varph::Electron, Q1CORE::Example::Varph::Spark}");

            EXPECT_EQ(schema.aspects.at(Facet<Atom>::typeId).invariants.own.size(), Atom::invariants.list.size());
            EXPECT_EQ(schema.aspects.at(Facet<Spark>::typeId).invariants.own.size(), Spark::invariants.list.size());
            EXPECT_EQ(schema.aspects.at(Facet<Chemical>::typeId).invariants.own.size(), Chemical::invariants.list.size());
        }
        { // merge two Schema's
            const auto a = ops::schema::assemble<Atom>();
            const auto b = ops::schema::assemble<Chemical>();

            const auto ab = SchemaObject::merge(a, b);
            const auto ba = SchemaObject::merge(b, a);

            EXPECT_TRUE(ab != nullptr);
            EXPECT_TRUE(ba != nullptr);

            EXPECT_EQ(ab->aspects.size(), 5);
            EXPECT_EQ(ba->aspects.size(), 5);

            EXPECT_TRUE(ab->aspects.contains(Facet<Spark>::typeId));
            EXPECT_TRUE(ab->aspects.contains(Facet<Atom>::typeId));
            EXPECT_TRUE(ab->aspects.contains(Facet<Chemical>::typeId));
            EXPECT_TRUE(ab->aspects.contains(Facet<Charge>::typeId));
            EXPECT_TRUE(ab->aspects.contains(Facet<Electron>::typeId));

            EXPECT_EQ(ab->aspects.at(Facet<Atom>::typeId).invariants.own.size(), Atom::invariants.list.size());
            EXPECT_EQ(ba->aspects.at(Facet<Atom>::typeId).invariants.own.size(), Atom::invariants.list.size());

            EXPECT_TRUE(ab->aspects.at(Facet<Spark>::typeId).required_by.contains(Facet<Atom>::typeId));
            EXPECT_TRUE(ab->aspects.at(Facet<Atom>::typeId).required_by.contains(Facet<Chemical>::typeId));
        }
    }
}
