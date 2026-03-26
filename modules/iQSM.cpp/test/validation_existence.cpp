#include "_common.h"

#include <Atomic/varph.q1.h>

namespace tests {
    void validation_existence() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        const World initial = ops::world::create(ops::schema::assemble<Electron>());
        auto tx = repo::Sequence{World{initial}};

        const auto s_neg = ops::particle::create<Spark>(tx, Spark::Quantum{vec4{0, 0, 0, 0}, eVt{0}});
        const auto s_zero = ops::particle::create<Spark>(tx, Spark::Quantum{vec4{1, 0, 0, 0}, eVt{0}});
        const auto s_none = ops::particle::create<Spark>(tx, Spark::Quantum{vec4{2, 0, 0, 0}, eVt{0}});

        ops::particle::create<Charge>(tx, s_neg, Charge::Quantum{integer{-1}});
        ops::particle::create<Charge>(tx, s_zero, Charge::Quantum{integer{0}});

        EXPECT_TRUE(not ops::particle::exists<Electron>(initial, s_neg));
        EXPECT_TRUE(not ops::particle::exists<Electron>(initial, s_zero));
        EXPECT_TRUE(not ops::particle::exists<Electron>(initial, s_none));

        const World next = ops::validate_smart(initial, ops::integrate(initial, tx.push()));

        EXPECT_TRUE(ops::particle::exists<Electron>(next, s_neg));
        EXPECT_TRUE(not ops::particle::exists<Electron>(next, s_zero));
        EXPECT_TRUE(not ops::particle::exists<Electron>(next, s_none));
    }
}

