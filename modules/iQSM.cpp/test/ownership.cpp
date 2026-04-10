#include "_common.h"

#include <Etalon/aspects.q1.h>

namespace tests {
    void ownership() {
        using namespace iqsm::dsl_gateway;
        using namespace Q1CORE::Etalon;

        repo::Branch master{ops::world::create(ops::schema::assemble<Remnant>())};
        const auto sampleEntity = ops::particle::create<SampleEntity>(master, SampleEntity::Quantum{
            .data_field = integer{7},
        });
        ops::global::modifier<Tag>(master)->modulus = integer{7};
        EXPECT_TRUE(ops::particle::exists<Remnant>(master, sampleEntity));

        const auto trivia = ops::particle::create<Trivia>(master, Trivia::Quantum{});
        ops::particle::modifier<Remnant>(master, sampleEntity)->trivia = trivia;
        EXPECT_TRUE(ops::particle::exists<Trivia>(master, trivia));

        ops::global::modifier<Tag>(master)->modulus = integer{5};
        EXPECT_FALSE(ops::particle::exists<Remnant>(master, sampleEntity));
        EXPECT_FALSE(ops::particle::exists<Trivia>(master, trivia));
    }
}

