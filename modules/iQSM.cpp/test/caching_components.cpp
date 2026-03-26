#include "_common.h"

#include <Atomic/varph.q1.h>

namespace tests {
    void caching_components() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        // Build a minimal schema needed for ionisation computation:
        // - Atom holds Nucleon ids (Spark ids confined by Nucleon aspect)
        // - Charge carries charge for each Spark
        // - Chemical is a component of Atom (1-1 iff)
        const World world = ops::world::create(ops::schema::assemble<Chemical, Charge>());
        auto tx = repo::Sequence{World{world}};

        const auto mkSpark = [&](integer charge) {
            const auto id = ops::particle::create<Spark>(tx, Spark::Quantum{vec4{0, 0, 0, 0}, eVt{0}});
            ops::particle::create<Charge>(tx, id, Charge::Quantum{charge});
            return id;
        };

        const auto mkNucleon = [&](integer charge, integer isospin2) {
            const auto id = mkSpark(charge);
            ops::particle::create<Strong>(tx, id, Strong::Quantum{isospin2});
            ops::particle::create<Nucleon>(tx, id, Nucleon::Quantum{""});
            return id;
        };

        const auto proton1 = mkNucleon(integer{+1}, integer{+1});
        const auto proton2 = mkNucleon(integer{+1}, integer{+1});
        const auto neutron1 = mkNucleon(integer{0}, integer{-1});
        const auto neutron2 = mkNucleon(integer{0}, integer{-1});
        const auto electronSpark1 = mkSpark(integer{-1});
        const auto electron1 = ops::particle::create<Electron>(tx, electronSpark1, Electron::Quantum{});

        // Helium ion (He+): 2p + 2n + 1e => total charge +1.
        const auto atomId = ops::particle::create<Atom>(tx, Atom::Quantum{
            std::vector<Nucleon::Id>{proton1, proton2, neutron1, neutron2},
            std::vector<Electron::Id>{electron1},
            "He+",
        });

        {
            EXPECT_TRUE(not ops::particle::exists<Chemical>(ops::integrate(world, tx.delta()), atomId));
        }

        const World next = ops::validate_smart(world, ops::integrate(world, tx.push()));

        EXPECT_TRUE(ops::particle::exists<Chemical>(next, atomId));
        const auto& chemical = ops::particle::get<Chemical>(next, atomId);
        EXPECT_EQ(chemical.ionisation, integer{1});
    }
}

