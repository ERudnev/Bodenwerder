#include "../_common.h"

#include <Etalon/aspects.q1.h>

namespace tests {
    void validation_element_validator_clamp() {
        using namespace iqsm::q1_gateway;
        using namespace Q1_iQSM::Etalon;

        repo::Branch master{ops::world::create_no_resources(ops::schema::assemble<SampleEntity>())};

        const auto too_small = ops::particle::create<SampleEntity>(master, SampleEntity::Quantum{.data_field = integer{-1005}});
        const auto too_large = ops::particle::create<SampleEntity>(master, SampleEntity::Quantum{.data_field = integer{1005}});
        const auto ok = ops::particle::create<SampleEntity>(master, SampleEntity::Quantum{.data_field = integer{17}});

        EXPECT_EQ(debug::read<SampleEntity>(master, too_small)->data_field, integer{-1000});
        EXPECT_EQ(debug::read<SampleEntity>(master, too_large)->data_field, integer{1000});
        EXPECT_EQ(debug::read<SampleEntity>(master, ok)->data_field, integer{17});
    }
}
