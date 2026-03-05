#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>

namespace tests {
    void caching_components() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        // Build a minimal schema needed for ionisation computation:
        // - Atom holds Spark ids
        // - Charge carries charge for each Spark
        // - Chemical is a component of Atom (1-1 iff)
        const World world = ops::world::create(ops::schema::assemble<Chemical, Charge>());
        ops::Transaction tx{World{world}};

        const auto mkSpark = [&](integer charge) {
            const auto id = ops::particle::create<Spark>(tx)({vec3{0, 0, 0}, eVt{0}});
            ops::particle::create<Charge>(tx, id)({charge});
            return id;
        };

        const auto proton1 = mkSpark(integer{+1});
        const auto proton2 = mkSpark(integer{+1});
        const auto neutron1 = mkSpark(integer{0});
        const auto neutron2 = mkSpark(integer{0});
        const auto electronSpark1 = mkSpark(integer{-1});
        const auto electron1 = ops::particle::create<Electron>(tx)({electronSpark1, "1s"});

        // Helium ion (He+): 2p + 2n + 1e => total charge +1.
        const auto atomId = ops::particle::create<Atom>(tx)({
            std::vector<Spark::Id>{proton1, proton2, neutron1, neutron2},
            std::vector<Electron::Id>{electron1},
            "He+",
        });

        {
            const World populated = tx.current;
            EXPECT_TRUE(not ops::particle::exists<Chemical>(populated, atomId));

            const auto delta = Chemical::invariants.apply(populated);
            const World next = ops::integrate_raw(populated, delta);

            EXPECT_TRUE(ops::particle::exists<Chemical>(next, atomId));
            const auto& chemical = ops::particle::get<Chemical>(next, atomId);
            EXPECT_EQ(chemical.ionisation, integer{1});
        }
    }
}

