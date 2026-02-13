#include <base/testing/macros.h>

#include <Atomic/model.q1.h>
#include <iQSM/schema.h>

namespace tests {
    void schema_aspects() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Model;
        auto schema = SchemaObject::assemble<Atom>();

        EXPECT_EQ(schema.aspects.size(), 3);

        const auto& atom = schema.aspects.at(Aspect<Atom>::typeId);
        EXPECT_TRUE(atom.require.contains(Aspect<Element>::typeId));
        EXPECT_TRUE(atom.require.contains(Aspect<Molecule>::typeId));
        EXPECT_EQ(atom.require.size(), 2);

        const auto& element = schema.aspects.at(Aspect<Element>::typeId);
        EXPECT_TRUE(element.required_by.contains(Aspect<Atom>::typeId));
        EXPECT_TRUE(element.zero != nullptr);

        const auto& body = schema.aspects.at(Aspect<Molecule>::typeId);
        EXPECT_TRUE(body.required_by.contains(Aspect<Atom>::typeId));
        EXPECT_TRUE(body.zero != nullptr);

        EXPECT_TRUE(atom.zero != nullptr);
    }
}


