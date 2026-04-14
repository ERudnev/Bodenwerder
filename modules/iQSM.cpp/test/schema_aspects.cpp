#include "_common.h"

#include <Etalon/aspects.q1.h>

namespace tests {
    void schema_aspects() {
        using namespace iqsm;
        using namespace Q1CORE::Etalon;

        const SchemaObject schema = SchemaObject::assemble<SampleAttribute>();

        EXPECT_EQ(schema.aspects.size(), 3u);

        const auto id_entity = types::aspectId<SampleEntity>();
        const auto id_component = types::aspectId<SampleComponent>();
        const auto id_attribute = types::aspectId<SampleAttribute>();

        const auto& attribute = schema.aspects.at(id_attribute);
        EXPECT_TRUE(attribute.require.contains(id_entity));
        EXPECT_TRUE(attribute.require.contains(id_component));
        EXPECT_EQ(attribute.require.size(), 2u);

        const auto& component = schema.aspects.at(id_component);
        EXPECT_TRUE(component.required_by.contains(id_attribute));

        const auto& entity = schema.aspects.at(id_entity);
        EXPECT_TRUE(entity.required_by.contains(id_component));
        EXPECT_TRUE(entity.required_by.contains(id_attribute));
    }
}
