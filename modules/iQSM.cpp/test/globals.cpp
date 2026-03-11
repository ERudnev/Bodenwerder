#include <base/testing/macros.h>

#include <iQSM/_all.include.h>

namespace tests {
    using namespace iqsm::dsl_gateway;

    struct FooWithGlobal : Entity<FooWithGlobal>, Require<> {
        struct Quantum {
            integer value = 0;
        };
        struct Global {
            integer tick = 0;
        };
        static const Invariants invariants;
    };

    const Invariants FooWithGlobal::invariants{{{
    }}};
}

namespace tests {
    void globals() {
        using namespace iqsm;
        using namespace iqsm::dsl_gateway;

        World world = ops::world::create(ops::schema::assemble<FooWithGlobal>());

        // Read global (field-own) data.
        EXPECT_EQ(ops::global::get<FooWithGlobal>(world)->tick, integer{0});

        // Modify global via transaction (illustrates syntax and merge of consecutive global changes).
        auto tx = ops::Transaction::integrator(std::move(world));
        {
            auto g = ops::global::modify<FooWithGlobal>(tx);
            g->tick = integer{1};
        }
        {
            auto g = ops::global::modify<FooWithGlobal>(tx);
            g->tick = integer{2};
        }

        world = tx.current;
        EXPECT_EQ(ops::global::get<FooWithGlobal>(world)->tick, integer{2});
    }
}

