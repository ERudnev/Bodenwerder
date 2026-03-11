#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>

namespace tests {
    void validation_existence() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        const World initial = ops::world::create(ops::schema::assemble<Electron>());
        auto tx = ops::Transaction::integrator(initial);

        const auto s_neg = ops::particle::create<Spark>(tx)({vec4{0, 0, 0, 0}, eVt{0}});
        const auto s_zero = ops::particle::create<Spark>(tx)({vec4{1, 0, 0, 0}, eVt{0}});
        const auto s_none = ops::particle::create<Spark>(tx)({vec4{2, 0, 0, 0}, eVt{0}});

        ops::particle::create<Charge>(tx, s_neg)({integer{-1}});
        ops::particle::create<Charge>(tx, s_zero)({integer{0}});

        EXPECT_TRUE(not ops::particle::exists<Electron>(initial, s_neg));
        EXPECT_TRUE(not ops::particle::exists<Electron>(initial, s_zero));
        EXPECT_TRUE(not ops::particle::exists<Electron>(initial, s_none));

        const World next = ops::integrate(initial, tx.summary);

        EXPECT_TRUE(ops::particle::exists<Electron>(next, s_neg));
        EXPECT_TRUE(not ops::particle::exists<Electron>(next, s_zero));
        EXPECT_TRUE(not ops::particle::exists<Electron>(next, s_none));
    }
}

