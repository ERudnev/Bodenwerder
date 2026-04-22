#include "../_common.h"

#include <iQSM/internals/delta_builders.h>

#include <Etalon/aspects.q1.h>

namespace tests {
    void merge_add_remove_constructor_issue() {
        /* OBSOLETE SYNTAX, update some day
        
        using namespace iqsm::dsl_gateway;
        using namespace Q1_iQSM::Etalon;

        repo::Branch master{ops::world::create_no_resources(ops::schema::assemble<Remnant>())};
        const auto sampleEntity = ops::particle::create<SampleEntity>(master, SampleEntity::Quantum{
            .data_field = integer{7},
        });

        repo::Branch branch_add{master};
        ops::global::modifier<Tag>(branch_add)->modulus = integer{7};
        EXPECT_TRUE(ops::particle::exists<Remnant>(branch_add, sampleEntity));
        const auto trivia = debug::read<Remnant>(branch_add, sampleEntity)->trivia;
        EXPECT_TRUE(ops::particle::exists<Trivia>(branch_add, trivia));

        repo::Sequence seq_add{master};
        seq_add.absorb(branch_add.delta());

        repo::Sequence seq_remove{master};
        ops::global::modifier<Tag>(seq_remove)->modulus = integer{5};
        seq_remove.absorb(iqsm::internals::delta::make_atomic<Tag>(
            sampleEntity,
            ops::particle::item<Tag>(branch_add, sampleEntity),
            std::nullopt));
        seq_remove.absorb(iqsm::internals::delta::make_atomic<Remnant>(
            sampleEntity,
            ops::particle::item<Remnant>(branch_add, sampleEntity),
            std::nullopt));

        repo::Accumulator merged{master};
        merged.absorb(seq_add.push());
        merged.absorb(seq_remove.push());
        merged.on_finish();

        EXPECT_FALSE(ops::particle::exists<Remnant>(master, sampleEntity));
        EXPECT_TRUE(ops::particle::exists<Trivia>(master, trivia));
        */
    }
}

