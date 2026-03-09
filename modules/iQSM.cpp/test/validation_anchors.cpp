#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>

namespace tests {
    void validation_anchors() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        // Scenario 1: Electron is confined to Charge (attribute of Charge)
        {
            ops::Transaction transaction(ops::world::create(ops::schema::assemble<Electron>()));
            const auto spark_ok = ops::particle::create<Spark>(transaction)({vec3{0, 0, 0}, eVt{1}});
            ops::particle::create<Charge>(transaction, spark_ok)({integer{-1}});

            const auto electron_ok = ops::particle::create<Electron>(transaction, spark_ok)({});
            const auto bad_id = Spark::Id::generate_random();
            const auto electron_bad = ops::particle::create<Electron>(transaction, bad_id)({});

            const auto populated_world = transaction.current;
            EXPECT_EQ(populated_world->field<Electron>()->container.size(), size_t{2});

            auto delta = ops::validation::Structural::anchor_attribute<Charge, Electron>(populated_world);

            auto next = ops::integrate_raw(populated_world, delta);
            EXPECT_EQ(next->field<Electron>()->container.size(), size_t{1});
            EXPECT_TRUE(ops::particle::exists<Electron>(next, electron_ok));
            EXPECT_TRUE(not ops::particle::exists<Electron>(next, electron_bad));
        }

        // Scenario 2: Capture.atom anchors to Atom
        {
            ops::Transaction transaction(ops::world::create(ops::schema::assemble<Capture>()));
            const auto atom_ok = ops::particle::create<Atom>(transaction)({{}, {}, "H"});
            const auto capture_ok = ops::particle::create<Capture>(transaction)({atom_ok, Electron::Id::generate_random(), "1s1", eVt{-13.6f}});
            const auto capture_bad = ops::particle::create<Capture>(transaction)({Atom::Id::generate_random(), Electron::Id::generate_random(), "1s1", eVt{-13.6f}});

            const auto populated_world = transaction.current;
            EXPECT_EQ(populated_world->field<Capture>()->container.size(), size_t{2});

            auto delta = ops::validation::Structural::anchor<Atom, Capture, &Capture::Quantum::atom>(populated_world);
            auto next = ops::integrate_raw(populated_world, delta);

            EXPECT_EQ(next->field<Capture>()->container.size(), size_t{1});
            EXPECT_TRUE(ops::particle::exists<Capture>(next, capture_ok));
            EXPECT_TRUE(not ops::particle::exists<Capture>(next, capture_bad));
        }

        // Scenario 3: Atom.core anchors to Nucleon (any)
        {
            ops::Transaction transaction(ops::world::create(ops::schema::assemble<Atom>()));
            const auto spark_ok = ops::particle::create<Spark>(transaction)({vec3{0, 0, 0}, eVt{1}});
            ops::particle::create<Strong>(transaction, spark_ok)({integer{+1}});
            ops::particle::create<Nucleon>(transaction, spark_ok)({"P"});

            const auto atom_keep = ops::particle::create<Atom>(transaction)({std::vector<Nucleon::Id>{spark_ok, Spark::Id::generate_random()}, {}, "H"});
            const auto atom_die = ops::particle::create<Atom>(transaction)({std::vector<Nucleon::Id>{Spark::Id::generate_random()}, {}, "He"});

            const auto populated_world = transaction.current;
            auto delta = ops::validation::Structural::anchor_any<Nucleon, Atom, &Atom::Quantum::core>(populated_world);
            auto next = ops::integrate_raw(populated_world, delta);

            EXPECT_EQ(next->field<Atom>()->container.size(), size_t{1});
            EXPECT_TRUE(ops::particle::exists<Atom>(next, atom_keep));

            const auto& kept = ops::particle::get<Atom>(next, atom_keep);
            EXPECT_EQ(kept.core.size(), size_t{1});
            EXPECT_TRUE(kept.core.at(0) == spark_ok);
        }

        // Scenario 4: Binding.bound anchors to Atom (all)
        {
            ops::Transaction transaction(ops::world::create(ops::schema::assemble<Binding>()));
            const auto atom_ok = ops::particle::create<Atom>(transaction)({{}, {}, "H"});
            const auto bind_ok = ops::particle::create<Binding>(transaction)({std::vector<Atom::Id>{atom_ok}});
            const auto bind_bad = ops::particle::create<Binding>(transaction)({std::vector<Atom::Id>{atom_ok, Atom::Id::generate_random()}});

            const auto populated_world = transaction.current;
            auto delta = ops::validation::Structural::anchor_all<Atom, Binding, &Binding::Quantum::bound>(populated_world);
            auto next = ops::integrate_raw(populated_world, delta);

            EXPECT_EQ(next->field<Binding>()->container.size(), size_t{1});
            EXPECT_TRUE(ops::particle::exists<Binding>(next, bind_ok));
            EXPECT_TRUE(not ops::particle::exists<Binding>(next, bind_bad));
        }
    }
}

