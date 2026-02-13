#include <base/testing/macros.h>

#include <Atomic/model.q1.h>
#include <iQSM/schema.h>
#include <iQSM/world.h>
#include <stdexcept>

namespace tests {
    void worlddata_construct() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Model;

        auto schema = std::make_shared<const SchemaObject>(SchemaObject::assemble<Atom>());
        EXPECT_TRUE(schema != nullptr);
        EXPECT_EQ(schema->aspects.size(), 3);

        World world = std::make_shared<const WorldObject>(schema);
        EXPECT_TRUE(world != nullptr);
        EXPECT_TRUE(world->schema == schema);

        auto element = world->field<Element>();
        EXPECT_TRUE(element != nullptr);
        EXPECT_EQ(element->container.size(), size_t{0});
    }

    void worlddata_closure_pulls_dependencies() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Model;

        auto schema = SchemaObject::assemble<Atom>();

        EXPECT_EQ(schema.aspects.size(), 3);
        EXPECT_TRUE(schema.aspects.contains(Aspect<Element>::typeId));
        EXPECT_TRUE(schema.aspects.contains(Aspect<Molecule>::typeId));
        EXPECT_TRUE(schema.aspects.contains(Aspect<Atom>::typeId));
    }
}


