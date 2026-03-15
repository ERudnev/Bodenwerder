#include "_common.h"

#include <Atomic/varph.q1.h>

namespace tests {
    void validation_anchors() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        // Scenario 1: Electron is confined to Charge (attribute of Charge)
        {
            World world = ops::world::create(ops::schema::assemble<Electron>());
            auto transaction = repo::Sequence{World{world}};
            const auto spark_ok = ops::particle::create<Spark>(transaction, Spark::Quantum{vec4{0, 0, 0, 0}, eVt{1}});
            ops::particle::create<Charge>(transaction, spark_ok, Charge::Quantum{integer{-1}});

            const auto electron_ok = ops::particle::create<Electron>(transaction, spark_ok, Electron::Quantum{});
            const auto bad_id = Spark::Id::generate_random();
            const auto electron_bad = ops::particle::create<Electron>(transaction, bad_id, Electron::Quantum{});

            world = static_cast<World>(transaction);
            EXPECT_EQ(world->field<Electron>()->container.size(), size_t{2});

            auto delta = ops::validation::Structural::anchor_attribute<Charge, Electron>(world);

            world = ops::integrate(world, delta);
            EXPECT_EQ(world->field<Electron>()->container.size(), size_t{1});
            EXPECT_TRUE(ops::particle::exists<Electron>(world, electron_ok));
            EXPECT_TRUE(not ops::particle::exists<Electron>(world, electron_bad));
        }

        // Scenario 2: Capture.atom anchors to Atom
        {
            auto transaction = repo::Sequence{ops::world::create(ops::schema::assemble<Capture>())};
            const auto atom_ok = ops::particle::create<Atom>(transaction, Atom::Quantum{{}, {}, "H"});
            const auto capture_ok = ops::particle::create<Capture>(transaction, Capture::Quantum{atom_ok, Electron::Id::generate_random(), "1s1", eVt{-13.6f}});
            const auto capture_bad = ops::particle::create<Capture>(transaction, Capture::Quantum{Atom::Id::generate_random(), Electron::Id::generate_random(), "1s1", eVt{-13.6f}});

            const auto populated_world = static_cast<World>(transaction);
            EXPECT_EQ(populated_world->field<Capture>()->container.size(), size_t{2});

            auto delta = ops::validation::Structural::anchor<Atom, Capture, &Capture::Quantum::atom>(populated_world);
            auto next = ops::integrate(populated_world, delta);

            EXPECT_EQ(next->field<Capture>()->container.size(), size_t{1});
            EXPECT_TRUE(ops::particle::exists<Capture>(next, capture_ok));
            EXPECT_TRUE(not ops::particle::exists<Capture>(next, capture_bad));
        }

        // Scenario 3: Atom.core anchors to Nucleon (any)
        {
            auto transaction = repo::Sequence{ops::world::create(ops::schema::assemble<Atom>())};
            const auto spark_ok = ops::particle::create<Spark>(transaction, Spark::Quantum{vec4{0, 0, 0, 0}, eVt{1}});
            ops::particle::create<Strong>(transaction, spark_ok, Strong::Quantum{integer{+1}});
            ops::particle::create<Nucleon>(transaction, spark_ok, Nucleon::Quantum{"P"});

            const auto atom_keep = ops::particle::create<Atom>(transaction, Atom::Quantum{std::vector<Nucleon::Id>{spark_ok, Spark::Id::generate_random()}, {}, "H"});
            const auto atom_die = ops::particle::create<Atom>(transaction, Atom::Quantum{std::vector<Nucleon::Id>{Spark::Id::generate_random()}, {}, "He"});

            const auto populated_world = static_cast<World>(transaction);
            auto delta = ops::validation::Structural::anchor_any<Nucleon, Atom, &Atom::Quantum::core>(populated_world);
            auto next = ops::integrate(populated_world, delta);

            EXPECT_EQ(next->field<Atom>()->container.size(), size_t{1});
            EXPECT_TRUE(ops::particle::exists<Atom>(next, atom_keep));

            const auto& kept = ops::particle::get<Atom>(next, atom_keep);
            EXPECT_EQ(kept.core.size(), size_t{1});
            EXPECT_TRUE(kept.core.at(0) == spark_ok);
        }

        // Scenario 4: Binding.bound anchors to Atom (all)
        {
            auto transaction = repo::Sequence{ops::world::create(ops::schema::assemble<Binding>())};
            const auto atom_ok = ops::particle::create<Atom>(transaction, Atom::Quantum{{}, {}, "H"});
            const auto bind_ok = ops::particle::create<Binding>(transaction, Binding::Quantum{std::vector<Atom::Id>{atom_ok}});
            const auto bind_bad = ops::particle::create<Binding>(transaction, Binding::Quantum{std::vector<Atom::Id>{atom_ok, Atom::Id::generate_random()}});

            const auto populated_world = static_cast<World>(transaction);
            auto delta = ops::validation::Structural::anchor_all<Atom, Binding, &Binding::Quantum::bound>(populated_world);
            auto next = ops::integrate(populated_world, delta);

            EXPECT_EQ(next->field<Binding>()->container.size(), size_t{1});
            EXPECT_TRUE(ops::particle::exists<Binding>(next, bind_ok));
            EXPECT_TRUE(not ops::particle::exists<Binding>(next, bind_bad));
        }
    }
}

