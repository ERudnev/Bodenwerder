#include "_common.h"

#include <iQSM/api/_gateway.h>

#include <iQSM/repository/branch.h>
#include <iQSM/repository/sequence.h>
#include <iQSM/repository/accumulator.h>

#include "utilities/fooBar.h"

namespace tests {

    namespace {
        iqsm::World seed_foos_oldschool(iqsm::World world, int count, int start_value) {
            using Foo = tests::utility::Foo;

            auto fd = base::make_shared<iqsm::delta::FieldDiff<Foo>>();

            for (int i = 0; i < count; ++i) {
                const auto id = iqsm::Id<Foo>::generate_random();

                typename iqsm::delta::FieldDiff<Foo>::Operation op{};
                op.after = iqsm::Facet<Foo>::create(Foo::Quantum{start_value + i});
                fd->ops.emplace(id, std::move(op));
            }

            auto wd = base::make_shared<iqsm::delta::Fields>();
            wd->fields.emplace(iqsm::Facet<Foo>::typeId, iqsm::freeze(fd));

            return iqsm::ops::integrate(world, iqsm::freeze(wd));
        }
    }

    void transaction_repo() {
        using namespace iqsm;
        using utility::Bar;
        using utility::Foo;

        repo::Branch master = repo::Branch{ops::world::create(ops::schema::assemble<Foo, Bar>())};

        master.rebase(seed_foos_oldschool(master, 10, 100));
        EXPECT_EQ(debug::count<Foo>(master), 10) << "rebase from non-delta patch";

        const auto id = ops::particle::create<Foo>(master, Foo::Quantum{10});
        EXPECT_TRUE(debug::read<Foo>(master, id).exists()) << "Commit applied as atomic branch update";

        ops::particle::modifier<Foo>(master, id)->value = 5;
        EXPECT_EQ(debug::read<Foo>(master, id)->value, 5) << "simple modification applied";

        {
            auto mod = ops::particle::modifier<Foo>(master, id);
            mod->value = mod->value - 1;
            mod->value = mod->value - 2;
            EXPECT_EQ(debug::read<Foo>(master, id)->value, 5) << "branch is unchanged until modifier scope ends";
        }
        EXPECT_EQ(debug::read<Foo>(master, id)->value, 2) << "modifier applied on scope exit";

        ops::particle::modifier<Foo>(master, id)->value = -5;
        EXPECT_TRUE(not debug::read<Foo>(master, id).exists()) << "negative Foo removed by validation";

        { // stress test: two concurrent changes
            repo::Branch fork(master); // expected conversion Branch->World
            const auto thorn = ops::particle::create<Foo>(fork, Foo::Quantum{10});
            {
                auto mod_a = ops::particle::modifier<Foo>(fork, thorn);
                auto mod_b = ops::particle::modifier<Foo>(fork, thorn);
                mod_a->value = 8;
                mod_b->value = 6;
            }
            EXPECT_EQ(debug::read<Foo>(fork, thorn)->value, 8) << "Last-wins is the only merge policy for now";
            master.absorb(fork.push());
        }

        { // Sequence: no validation until absorbed into Branch
            const size_t before = debug::count<Foo>(master);
            repo::Sequence seq{master};
            for (int v = -5; v <= 5; ++v) {
                ops::particle::create<Foo>(seq, Foo::Quantum{v});
            }

            EXPECT_EQ(debug::count<Foo>(seq) - before, size_t{11}) << "absorbing to Sequence as is, no validation called";

            master.absorb(seq.push());
            EXPECT_EQ(debug::count<Foo>(master), before + size_t{6}) << "validation runs in Branch::absorb; invalid Foo's must not enter master";
        }

        { // Accumulator: no integrate/validate; validation happens on absorb into Branch
            const size_t before = debug::count<Foo>(master);

            repo::Accumulator acc{master};
            const size_t head_count = debug::count<Foo>(acc);
            const size_t assumed_count = debug::count<Foo>(acc);
            const int computed = static_cast<int>(assumed_count) - static_cast<int>(head_count) - 1;

            for (int i = 0; i < 11; ++i) {
                ops::particle::create<Foo>(acc, Foo::Quantum{integer{computed}});
            }

            EXPECT_EQ(debug::count<Foo>(acc), head_count) << "Accumulator must not integrate head while accumulating";

            master.absorb(acc.push());
            EXPECT_EQ(debug::count<Foo>(master), before) << "validation runs in Branch::absorb; all accumulated Foo's are invalid (-1) and must be removed";
        }
    }
}
