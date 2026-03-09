#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>

namespace tests {
    void transient_usage() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        // Seed a world with a hadron first, then start a "clean" transaction
        // on top of that world. This keeps `transaction.summary` empty at the
        // start of the scenario, so we can test "const access produces no delta".
        ops::Transaction seeding_transaction(ops::world::create(ops::schema::assemble<Hadron>()));
        const auto spark = ops::particle::create<Spark>(seeding_transaction)({vec3{0, 0, 0}, eVt{1}});
        ops::particle::create<Strong>(seeding_transaction, spark)({integer{+1}});
        const auto hadron = ops::particle::create<Hadron>(seeding_transaction, spark)({"A"});

        const World seeded_world = seeding_transaction.current;
        ops::Transaction transaction{World{seeded_world}};

        EXPECT_TRUE(transaction.summary->empty());

        // Read-only access via get() produces no delta.
        {
            const auto& hadron_const = ops::particle::get<Hadron>(transaction.current, hadron);
            EXPECT_EQ(hadron_const.legend, "A");
        }
        EXPECT_TRUE(transaction.summary->empty());
        EXPECT_EQ(ops::particle::get<Hadron>(transaction.current, hadron).legend, "A");

        // Creating modify() handle is treated as a mutation => delta is produced,
        // even if the resulting value is structurally identical and the handle is unused.
        {
            auto unused = ops::particle::modify<Hadron>(transaction, hadron);
        }
        EXPECT_TRUE(not transaction.summary->empty());
        EXPECT_EQ(ops::particle::get<Hadron>(transaction.current, hadron).legend, "A");

        // A named modify() handle may live for a while; apply happens on scope exit.
        {
            auto m = ops::particle::modify<Hadron>(transaction, hadron);
            m->legend = "B";
            m->legend = "C";
            (*m).legend = "D";
            m->legend = "E";
        }
        EXPECT_EQ(ops::particle::get<Hadron>(transaction.current, hadron).legend, "E");


        // Mutation marks dirty => delta is produced and transaction world changes.
        ops::particle::modify<Hadron>(transaction, hadron)->legend = "B";
        EXPECT_TRUE(not transaction.summary->empty());
        EXPECT_EQ(ops::particle::get<Hadron>(transaction.current, hadron).legend, "B");

        const auto untyped = transaction.summary->fields.at(Facet<Hadron>::typeId);
        const auto fd = base::shared_ref_cast<const delta::FieldDiff<Hadron>>(untyped);
        EXPECT_TRUE(fd->changed.contains(hadron));
        EXPECT_EQ(fd->changed.at(hadron).before->legend, "A");
        EXPECT_EQ(fd->changed.at(hadron).after->legend, "B");

        // Move does not double-apply (would throw on "before mismatch").
        {
            auto hadron_to_move = ops::particle::modify<Hadron>(transaction, hadron);
            hadron_to_move->legend = "C";
            auto moved_hadron = std::move(hadron_to_move);
            EXPECT_EQ(moved_hadron->legend, "C");
        }
        EXPECT_EQ(ops::particle::get<Hadron>(transaction.current, hadron).legend, "C");
    }
}

