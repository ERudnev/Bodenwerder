#include "../_common.h"

#include <Etalon/aspects.q1.h>

namespace tests {
    void validation_caching_component() {
        using namespace iqsm::dsl_gateway;
        using namespace Q1CORE::Etalon;

        repo::Branch master{ops::world::create(ops::schema::assemble<SampleEntity, Tag, Remnant>())};
        ops::global::modifier<Tag>(master)->modulus = integer{7};

        const auto divisible = ops::particle::create<SampleEntity>(master, SampleEntity::Quantum{.data_field = integer{14}});
        const auto not_divisible = ops::particle::create<SampleEntity>(master, SampleEntity::Quantum{.data_field = integer{15}});

        EXPECT_EQ(debug::count<Tag>(master), std::size_t{1});
        EXPECT_EQ(debug::count<Remnant>(master), std::size_t{1});

        EXPECT_TRUE(ops::particle::exists<Tag>(master, divisible));
        EXPECT_FALSE(ops::particle::exists<Tag>(master, not_divisible));

        EXPECT_TRUE(ops::particle::exists<Remnant>(master, divisible));
        EXPECT_FALSE(ops::particle::exists<Remnant>(master, not_divisible));

        const auto remnant = debug::read<Remnant>(master, divisible);
        EXPECT_TRUE(remnant.exists());
        EXPECT_EQ(remnant->power, integer{2});
    }
}
