#include <base/testing/macros.h>

#include <iQSM/schema.h>

#include <Atomic/varph.q1.h>


namespace tests {
    void schema_aspects() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;
        auto schema = SchemaObject::assemble<Atom>();

        // Atom pulls its dependencies transitively (closure)
        EXPECT_EQ(schema.aspects.size(), 4);

        const auto& atom = schema.aspects.at(Facet<Atom>::typeId);
        EXPECT_TRUE(atom.require.contains(Facet<Spark>::typeId));
        EXPECT_TRUE(atom.require.contains(Facet<Charge>::typeId));
        EXPECT_TRUE(atom.require.contains(Facet<Electron>::typeId));
        EXPECT_EQ(atom.require.size(), 3);

        const auto& spark = schema.aspects.at(Facet<Spark>::typeId);
        EXPECT_TRUE(spark.required_by.contains(Facet<Atom>::typeId));
        EXPECT_TRUE(spark.required_by.contains(Facet<Charge>::typeId));
        EXPECT_TRUE(spark.required_by.contains(Facet<Electron>::typeId));
        EXPECT_TRUE(spark.zero != nullptr);

        EXPECT_TRUE(atom.zero != nullptr);
    }
}

