#include <base/testing/macros.h>

#include <iQSM/schema.h>

#include <Atomic/varph.q1.h>


namespace tests {
    void schema_aspects() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;
        auto schema = SchemaObject::assemble<Atom>();

        EXPECT_EQ(schema.aspects.size(), 2);

        const auto& atom = schema.aspects.at(Facet<Atom>::typeId);
        EXPECT_TRUE(atom.require.contains(Facet<Spark>::typeId));
        EXPECT_EQ(atom.require.size(), 1);

        const auto& spark = schema.aspects.at(Facet<Spark>::typeId);
        EXPECT_TRUE(spark.required_by.contains(Facet<Atom>::typeId));
        EXPECT_TRUE(spark.zero != nullptr);

        EXPECT_TRUE(atom.zero != nullptr);
    }
}

