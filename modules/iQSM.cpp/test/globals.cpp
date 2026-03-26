#include "_common.h"

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

        const World before = ops::world::create(ops::schema::assemble<FooWithGlobal>());

        // Read global (field-own) data.
        EXPECT_EQ(ops::global::get<FooWithGlobal>(before)->tick, integer{0});

        // Modify global via transaction (illustrates syntax and merge of consecutive global changes).
        auto tx = repo::Sequence{before};
        ops::global::modifier<FooWithGlobal>(tx)->tick = integer{1};
        ops::global::modifier<FooWithGlobal>(tx)->tick = integer{2};

        const World validated = ops::validate_smart(before, ops::integrate(before, tx.push()));
        EXPECT_EQ(ops::global::get<FooWithGlobal>(validated)->tick, integer{2});
    }
}

