#include "_common.h"

#include <iQSM/schema.h>

#include <Atomic/varph.q1.h>


namespace tests {
    void schema_aspects() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;
        auto schema = SchemaObject::assemble<Atom>();

        // Assemble builds a closed schema (transitive closure of aspects),
        // but each Entry::require stores direct dependencies only.
        EXPECT_EQ(schema.aspects.size(), 6);

        const auto& atom = schema.aspects.at(Facet<Atom>::typeId);
        EXPECT_TRUE(atom.require.contains(Facet<Nucleon>::typeId));
        EXPECT_TRUE(atom.require.contains(Facet<Electron>::typeId));
        EXPECT_EQ(atom.require.size(), 2);

        const auto& spark = schema.aspects.at(Facet<Spark>::typeId);
        EXPECT_TRUE(spark.required_by.contains(Facet<Charge>::typeId));
        EXPECT_TRUE(spark.required_by.contains(Facet<Strong>::typeId));
    }
}

