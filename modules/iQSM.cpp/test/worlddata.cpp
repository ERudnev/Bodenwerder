#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>
#include <stdexcept>

namespace tests {
    void worlddata_construct() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        const World world = ops::world::create(ops::schema::assemble<Atom>());
        EXPECT_TRUE(world != nullptr);
        EXPECT_TRUE(world->schema != nullptr);
        EXPECT_EQ(world->schema->aspects.size(), 2);

        auto sparks = world->field<Spark>();
        EXPECT_TRUE(sparks != nullptr);
        EXPECT_EQ(sparks->container.size(), size_t{0});
    }

    void worlddata_closure_pulls_dependencies() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        auto schema = SchemaObject::assemble<Atom>();

        EXPECT_EQ(schema.aspects.size(), 2);
        EXPECT_TRUE(schema.aspects.contains(Facet<Spark>::typeId));
        EXPECT_TRUE(schema.aspects.contains(Facet<Atom>::typeId));
    }
}


