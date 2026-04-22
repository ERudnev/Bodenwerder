#include "_common.h"

#include <Etalon/aspects.q1.h>

namespace tests {
    void complex_constructor() {
        using namespace iqsm::dsl_gateway;
        using namespace Q1_iQSM::Etalon;

        const auto schema = ops::schema::assemble<SampleEntity, SampleComponent, SampleAttribute>();
        repo::Branch master{ ops::world::create_no_resources(schema) };

        const auto existing = ops::particle::create<SampleEntity>(master, SampleEntity::Quantum{
            .data_field = integer{1},
        });

        const auto created = SampleAttribute::Operations::create_complex_constructor(master, existing);

        EXPECT_TRUE(ops::particle::exists<SampleEntity>(master, created));
        EXPECT_TRUE(ops::particle::exists<SampleComponent>(master, existing));
        EXPECT_TRUE(ops::particle::exists<SampleComponent>(master, created));
        EXPECT_TRUE(ops::particle::exists<SampleAttribute>(master, created));

        const auto& created_entity = ops::particle::get<SampleEntity>(master, created);
        EXPECT_EQ((created_entity.data_field % integer{2}), integer{0});

        const auto& created_attribute = ops::particle::get<SampleAttribute>(master, created);
        EXPECT_EQ(created_attribute.neighbor_anchor, existing);
        EXPECT_TRUE(not created_attribute.optional_anchor.has_value());
        EXPECT_TRUE(created_attribute.every_essential.empty());
        EXPECT_EQ(created_attribute.at_least_one_required.size(), std::size_t{1});
        EXPECT_EQ(created_attribute.at_least_one_required[0], existing);
    }
}

