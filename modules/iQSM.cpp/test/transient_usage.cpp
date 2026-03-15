#include "_common.h"

#include <Atomic/varph.q1.h>

namespace tests {
    void transient_usage() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        // Seed a world with a hadron first, then start a "clean" transaction
        // on top of that world. This keeps `transaction.summary` empty at the
        // start of the scenario, so we can test "const access produces no delta".
        auto seeding_transaction = ops::Transaction::integrator(ops::world::create(ops::schema::assemble<Nucleon>()));
        const auto spark = ops::particle::create<Spark>(seeding_transaction, Spark::Quantum{vec4{0, 0, 0, 0}, eVt{1}});
        ops::particle::create<Strong>(seeding_transaction, spark, Strong::Quantum{integer{+1}});
        const auto nucleon = ops::particle::create<Nucleon>(seeding_transaction, spark, Nucleon::Quantum{"A"});

        World seeded_world = seeding_transaction.world;
        auto transaction = ops::Transaction::integrator(std::move(seeded_world));

        EXPECT_TRUE(transaction.summary()->empty());

        // Read-only access via get() produces no delta.
        {
            const auto& nucleon_const = ops::particle::get<Nucleon>(transaction.world, nucleon);
            EXPECT_EQ(nucleon_const.legend, "A");
        }
        EXPECT_TRUE(transaction.summary()->empty());
        EXPECT_EQ(ops::particle::get<Nucleon>(transaction.world, nucleon).legend, "A");

        // Creating modifier() handle is treated as a mutation => delta is produced,
        // even if the resulting value is structurally identical and the handle is unused.
        {
            auto unused = ops::particle::modifier<Nucleon>(transaction, nucleon);
        }
        EXPECT_TRUE(not transaction.summary()->empty());
        EXPECT_EQ(ops::particle::get<Nucleon>(transaction.world, nucleon).legend, "A");

        // A named modifier() handle may live for a while; apply happens on scope exit.
        {
            auto m = ops::particle::modifier<Nucleon>(transaction, nucleon);
            m->legend = "B";
            m->legend = "C";
            (*m).legend = "D";
            m->legend = "E";
        }
        EXPECT_EQ(ops::particle::get<Nucleon>(transaction.world, nucleon).legend, "E");


        // Mutation marks dirty => delta is produced and transaction world changes.
        ops::particle::modifier<Nucleon>(transaction, nucleon)->legend = "B";
        EXPECT_TRUE(not transaction.summary()->empty());
        EXPECT_EQ(ops::particle::get<Nucleon>(transaction.world, nucleon).legend, "B");

        const auto summary = transaction.summary();
        const auto untyped = summary->fields.at(Facet<Nucleon>::typeId);
        const auto fd = base::shared_ref_cast<const delta::FieldDiff<Nucleon>>(untyped);
        EXPECT_TRUE(fd->ops.contains(nucleon));
        const auto op = fd->ops.at(nucleon);
        EXPECT_TRUE(op.change.has_value());
        EXPECT_EQ(op.change->first->legend, "A");
        EXPECT_EQ(op.change->second->legend, "B");

        // Move does not double-apply (would throw on "before mismatch").
        {
            auto nucleon_to_move = ops::particle::modifier<Nucleon>(transaction, nucleon);
            nucleon_to_move->legend = "C";
            auto moved_nucleon = std::move(nucleon_to_move);
            EXPECT_EQ(moved_nucleon->legend, "C");
        }
        EXPECT_EQ(ops::particle::get<Nucleon>(transaction.world, nucleon).legend, "C");
    }
}

