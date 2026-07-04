#include "_common.h"

#include <format>
#include <string>
#include <vector>

#include <fQSM/api/interface.h>

namespace {
namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum { integer value; };
        using Reactions = DefaultReactions;
    };

    struct B : Component<B, A> {
        struct Quantum { string name; };
        using Reactions = DefaultReactions;
    };

    struct Arch : Archetype<Arch> {
        static void spawn(Writing context, int number, std::string str) {
            const auto id = ask::item::create<A>(context, { .value = number });
            ask::item::create<B>(context, id, { .name = std::move(str) });
        }
    };

}
} // namespace

namespace tests {

void transaction_hierarchy()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
    });

    context::Realm main(schema);


    // trivial call in Realm->Writing context
    with<Arch>::spawn(main, 7, "seven");
    EXPECT_EQ(debug::count<A>(main), 1);
    EXPECT_EQ(debug::count<B>(main), 1);

    { // same call, scoped
        with<Arch>::spawn(main, 17, "seventeen");
    }
    EXPECT_EQ(debug::count<A>(main), 2);
    EXPECT_EQ(debug::count<B>(main), 2);

    { // Branch changes will be applied with RAII (exiting this scope)
        context::Branch tx(main);
        with<Arch>::spawn(tx, 27, "twenty-seven");

        // Branch sees base + patch, Realm stays stable.
        EXPECT_EQ(debug::count<A>(tx), 3);
        EXPECT_EQ(debug::count<B>(tx), 3);
        EXPECT_EQ(debug::count<A>(main), 2);
        EXPECT_EQ(debug::count<B>(main), 2);
    }
    EXPECT_EQ(debug::count<A>(main), 3);
    EXPECT_EQ(debug::count<B>(main), 3);

    { // 10 workers are branches: each will make their job independently, which will be applied after all
        std::vector<context::Branch> workers;
        for (int xx = 0; xx < 10; ++xx)
            workers.emplace_back(context::Branch(main));

        // while each worker does its job, actual world state in realm is constant, so it may work in MT environment
        for (int xx = 0; xx < 10; ++xx) {
            auto& tx = workers[xx];
            for (int yy = 0; yy < 4; ++yy)
                with<Arch>::spawn(tx, 1000 + xx * 100 + yy, std::format("series: {}, step: {}", xx, yy));

            // Each worker sees base + its own patch (4 entities/components).
            EXPECT_EQ(debug::count<A>(tx), 7);
            EXPECT_EQ(debug::count<B>(tx), 7);
            EXPECT_EQ(debug::count<A>(main), 3);
            EXPECT_EQ(debug::count<B>(main), 3);
        }
    }

    // 3 (previous) + 10 * 4 (workers).
    EXPECT_EQ(debug::count<A>(main), 43);
    EXPECT_EQ(debug::count<B>(main), 43);
}

} // namespace tests
