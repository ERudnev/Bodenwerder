#include "_common.h"
#include "minimodel/aspects.q1.h"

#include <format>
#include <string>
#include <vector>

#include <fQSM/api/interface.h>
#include <fQSM/state/world/data.h>

namespace {
    using namespace ::tests::model;
    using namespace fqsm::api;

    void experimental_manipulator(fqsm::Writing context, int number, std::string str) {
        const auto id = ask::item::create<SomeEntity>(context, { .value = number });
        ask::item::create<SomeComponent>(context, id, { .name = str });
    }
}

namespace tests {

void transaction_hierarchy()
{
    using namespace ::tests::model;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<SomeEntity>(),
        ask::schema::aspect<SomeComponent>(),
    });

    fqsm::state::world::Data world(schema);
    context::Realm main(world);

    { // 2 trivial syncronous changes.. each with potentially heavy integration/normalization
        const auto id = ask::item::create<SomeEntity>(main, {7});
        ask::item::create<SomeComponent>(main, id, {"seven"});
    }
    EXPECT_EQ(debug::count<SomeEntity>(main), 1);
    EXPECT_EQ(debug::count<SomeComponent>(main), 1);

    { // immediate complex change:
        experimental_manipulator(main, 17, "seventeen");
    }
    EXPECT_EQ(debug::count<SomeEntity>(main), 2);
    EXPECT_EQ(debug::count<SomeComponent>(main), 2);

    { // Branch changes will be applied with RAII (exiting this scope)
        context::Branch tx(main);
        experimental_manipulator(tx, 27, "twenty-seven");

        // Branch sees base + patch, Realm stays stable.
        EXPECT_EQ(debug::count<SomeEntity>(tx), 3);
        EXPECT_EQ(debug::count<SomeComponent>(tx), 3);
        EXPECT_EQ(debug::count<SomeEntity>(main), 2);
        EXPECT_EQ(debug::count<SomeComponent>(main), 2);
    }
    EXPECT_EQ(debug::count<SomeEntity>(main), 3);
    EXPECT_EQ(debug::count<SomeComponent>(main), 3);

    { // 10 workers are branches: each will make their job independently, which will be applied after all
        std::vector<context::Branch> workers;
        for (int xx = 0; xx < 10; ++xx)
            workers.emplace_back(context::Branch(main));

        // while each worker does its job, actual world state in realm is constant, so it may work in MT environment
        for (int xx = 0; xx < 10; ++xx) {
            auto& tx = workers[xx];
            for (int yy = 0; yy < 4; ++yy)
                experimental_manipulator(tx, 1000 + xx * 100 + yy, std::format("series: {}, step: {}", xx, yy));

            // Each worker sees base + its own patch (4 entities/components).
            EXPECT_EQ(debug::count<SomeEntity>(tx), 7);
            EXPECT_EQ(debug::count<SomeComponent>(tx), 7);
            EXPECT_EQ(debug::count<SomeEntity>(main), 3);
            EXPECT_EQ(debug::count<SomeComponent>(main), 3);
        }
    }

    // 3 (previous) + 10 * 4 (workers).
    EXPECT_EQ(debug::count<SomeEntity>(main), 43);
    EXPECT_EQ(debug::count<SomeComponent>(main), 43);
}

} // namespace tests
