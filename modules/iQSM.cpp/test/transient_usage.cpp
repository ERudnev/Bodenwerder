#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>

namespace tests {
    void transient_usage() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        const auto schema = ops::schema::assemble<Electron>();
        // Seed a world with an electron first, then start a "clean" transaction
        // on top of that world. This keeps `transaction.summary == nullptr` at
        // the start of the scenario, so we can test "const access produces no delta".
        ops::Transaction seeding_transaction(ops::world::create(schema));
        const auto spark = ops::particle::create<Spark>(seeding_transaction)({vec3{0, 0, 0}, eVt{1}});
        const auto electron = ops::particle::create<Electron>(seeding_transaction)({spark, "A"});

        const World seeded_world = seeding_transaction.current;
        ops::Transaction transaction{World{seeded_world}};

        EXPECT_TRUE(transaction.summary == nullptr);

        // Const access does not mark dirty => no delta is produced.
        {
            const auto& electron_const = ops::particle::modify<Electron>(transaction, electron);
            EXPECT_EQ(electron_const->legend, "A");
        }
        EXPECT_TRUE(transaction.summary == nullptr);
        EXPECT_EQ(ops::particle::get<Electron>(transaction.current, electron)->legend, "A");

        // Mutation marks dirty => delta is produced and transaction world changes.
        ops::particle::modify<Electron>(transaction, electron)->legend = "B";
        EXPECT_TRUE(transaction.summary != nullptr);
        EXPECT_EQ(ops::particle::get<Electron>(transaction.current, electron)->legend, "B");

        const auto untyped = transaction.summary->fields.at(Aspect<Electron>::typeId);
        const auto fd = std::dynamic_pointer_cast<const delta::FieldDiff<Electron>>(untyped);
        EXPECT_TRUE(fd != nullptr);
        EXPECT_TRUE(fd->changed.contains(electron));
        EXPECT_EQ(fd->changed.at(electron).before->legend, "A");
        EXPECT_EQ(fd->changed.at(electron).after->legend, "B");

        // Move does not double-apply (would throw on "before mismatch").
        {
            auto electron_to_move = ops::particle::modify<Electron>(transaction, electron);
            electron_to_move->legend = "C";
            auto moved_electron = std::move(electron_to_move);
            EXPECT_EQ(moved_electron->legend, "C");
        }
        EXPECT_EQ(ops::particle::get<Electron>(transaction.current, electron)->legend, "C");
    }
}

