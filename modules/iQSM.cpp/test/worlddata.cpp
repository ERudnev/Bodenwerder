#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>
#include <stdexcept>

namespace tests {
    void worlddata_construct() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        const World world = ops::world::create(ops::schema::assemble<Atom>());
        EXPECT_EQ(world->schema->aspects.size(), 6);

        auto sparks = world->field<Spark>();
        EXPECT_EQ(sparks->container.size(), size_t{0});

        auto charges = world->field<Charge>();
        EXPECT_EQ(charges->container.size(), size_t{0});

        auto electrons = world->field<Electron>();
        EXPECT_EQ(electrons->container.size(), size_t{0});

        auto strong = world->field<Strong>();
        EXPECT_EQ(strong->container.size(), size_t{0});

        auto hadrons = world->field<Hadron>();
        EXPECT_EQ(hadrons->container.size(), size_t{0});
    }

    void worlddata_closure_pulls_dependencies() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        auto schema = SchemaObject::assemble<Atom>();

        EXPECT_EQ(schema.aspects.size(), 6);
        EXPECT_TRUE(schema.aspects.contains(Facet<Spark>::typeId));
        EXPECT_TRUE(schema.aspects.contains(Facet<Charge>::typeId));
        EXPECT_TRUE(schema.aspects.contains(Facet<Electron>::typeId));
        EXPECT_TRUE(schema.aspects.contains(Facet<Strong>::typeId));
        EXPECT_TRUE(schema.aspects.contains(Facet<Hadron>::typeId));
        EXPECT_TRUE(schema.aspects.contains(Facet<Atom>::typeId));
    }
}


