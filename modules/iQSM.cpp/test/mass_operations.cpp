#include "_common.h"

#include <Etalon/aspects.q1.h>

namespace tests {

    void mass_operations() {
        using namespace iqsm::dsl_gateway;
        using namespace Q1CORE::Etalon;

        repo::Branch branch{ops::world::create_no_resources(ops::schema::assemble<SampleEntity, SampleComponent>())};

        for (int i = 0; i < 100; ++i) {
            ops::particle::create<SampleEntity>(branch, SampleEntity::Quantum{integer{i}});
        }

        for (const auto& kv : branch->field<SampleEntity>()->container) {
            SampleComponent::Operations::example_op_multiply(branch, kv.first, integer{2});
        }

        // sum(0..99) = 4950; after *2 on every entity => 9900
        EXPECT_EQ(SampleEntity::Operations::example_const_fieldwide_method(branch), integer{9900});

        ops::particle::massop<SampleComponent>(branch, &SampleComponent::Operations::example_op_multiply, integer{2});

        // second *2 on all: 4 * sum(0..99) = 19800
        EXPECT_EQ(SampleEntity::Operations::example_const_fieldwide_method(branch), integer{19800});
    }
}
