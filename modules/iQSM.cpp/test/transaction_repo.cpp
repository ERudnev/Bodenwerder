#include "_common.h"

#include <iQSM/helpers/_experimental.h>
#include <iQSM/repository/branch.h>

#include "utilities/fooBar.h"

namespace tests {
    void transaction_repo() {
        using namespace iqsm;
        using utility::Bar;
        using utility::Foo;

        repo::Branch master = repo::Branch{ops::world::create(ops::schema::assemble<Foo, Bar>())};

        const auto id = ops::experimental::particle::create<Foo>(master, Foo::Quantum{10});
        EXPECT_TRUE(debug::read<Foo>(master, id).exists()) << "Commit applied as atomic branch update";

        ops::experimental::particle::modifier<Foo>(master, id)->value = 5;
        EXPECT_EQ(debug::read<Foo>(master, id)->value, 5) << "simple modification applied";

        {
            auto mod = ops::experimental::particle::modifier<Foo>(master, id);
            mod->value = mod->value - 1;
            mod->value = mod->value - 2;
            EXPECT_EQ(debug::read<Foo>(master, id)->value, 5) << "branch is unchanged until modifier scope ends";
        }
        EXPECT_EQ(debug::read<Foo>(master, id)->value, 2) << "modifier applied on scope exit";

        ops::experimental::particle::modifier<Foo>(master, id)->value = -5;
        EXPECT_TRUE(not debug::read<Foo>(master, id).exists()) << "negative Foo removed by validation";

        { // stress test: two concurrent changes
            repo::Branch fork(master); // expected conversion Branch->World
            const auto thorn = ops::experimental::particle::create<Foo>(fork, Foo::Quantum{10});
            {
                auto mod_a = ops::experimental::particle::modifier<Foo>(fork, thorn);
                auto mod_b = ops::experimental::particle::modifier<Foo>(fork, thorn);
                mod_a->value = 8;
                mod_b->value = 6;
            }
            EXPECT_EQ(debug::read<Foo>(fork, thorn)->value, 8) << "Last-wins is the only merge policy for now";
            master.absorb(fork.push());
        }
    }
}
