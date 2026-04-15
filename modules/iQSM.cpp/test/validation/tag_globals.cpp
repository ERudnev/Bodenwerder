#include "../_common.h"

#include <Etalon/aspects.q1.h>

namespace tests {
    void validation_tag_globals() {
        using namespace iqsm::dsl_gateway;
        using namespace Q1CORE::Etalon;

        const auto empty = ops::world::create_no_resources(ops::schema::assemble<SampleEntity, Tag>());
        repo::Branch master{empty};

        // create 100 entities with data_field = 1..100
        repo::Sequence seq{master};
        for (int i = 1; i <= 100; ++i) {
            ops::particle::create<SampleEntity>(seq, SampleEntity::Quantum{integer{i}});
        }
        master.absorb(seq.push());

        EXPECT_EQ(debug::count<Tag>(master), std::size_t{50});

        { auto g = ops::global::modifier<Tag>(master); g->modulus = integer{4}; }
        EXPECT_EQ(debug::count<Tag>(master), std::size_t{25});

        { auto g = ops::global::modifier<Tag>(master); g->modulus = integer{3}; }
        EXPECT_EQ(debug::count<Tag>(master), std::size_t{33});

        { auto g = ops::global::modifier<Tag>(master); g->modulus = integer{2}; }
        EXPECT_EQ(debug::count<Tag>(master), std::size_t{50});
    }
}

