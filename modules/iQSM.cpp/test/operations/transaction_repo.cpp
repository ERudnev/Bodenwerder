#include "../_common.h"

#include <iQSM/internals/delta_builders.h>

#include <Etalon/aspects.q1.h>

namespace tests {

    namespace {
        iqsm::World seed_foos_oldschool(iqsm::World world, int count, int start_value) {
            using Foo = Q1CORE::Etalon::SampleEntity;
            using Quantum = iqsm::Quantum<Foo>;

            for (int i = 0; i < count; ++i) {
                const auto id = iqsm::Id<Foo>::generate_random();
                auto after = base::make_shared<const Quantum>(Quantum{::iqsm::q1::integer{start_value + i}});
                world = ::iqsm::operations::integrate(std::move(world),
                    ::iqsm::internals::delta::make_atomic<Foo>(id, std::nullopt, std::move(after)));
            }

            return world;
        }
    }

    void transaction_repo() {
        using namespace iqsm::dsl_gateway;
        using namespace Q1CORE::Etalon;

        using Foo = SampleEntity;
        using Bar = SampleAttribute;

        repo::Branch master = repo::Branch{ops::world::create_no_resources(ops::schema::assemble<Foo, Bar>())};

        master.rebase(seed_foos_oldschool(master, 10, 100));
        EXPECT_EQ(debug::count<Foo>(master), 10) << "rebase from non-delta patch";

        const auto id = ops::particle::create<Foo>(master, Foo::Quantum{integer{10}});
        EXPECT_TRUE(debug::read<Foo>(master, id).exists()) << "Commit applied as atomic branch update";

        ops::particle::modifier<Foo>(master, id)->data_field = integer{5};
        EXPECT_EQ(debug::read<Foo>(master, id)->data_field, integer{5}) << "simple modification applied";

        {
            auto mod = ops::particle::modifier<Foo>(master, id);
            mod->data_field = mod->data_field - integer{1};
            mod->data_field = mod->data_field - integer{2};
            EXPECT_EQ(debug::read<Foo>(master, id)->data_field, integer{5}) << "branch is unchanged until modifier scope ends";
        }
        EXPECT_EQ(debug::read<Foo>(master, id)->data_field, integer{2}) << "modifier applied on scope exit";

        ops::particle::modifier<Foo>(master, id)->data_field = integer{-5};
        EXPECT_TRUE(debug::read<Foo>(master, id).exists()) << "etalon validators are degenerate; negative Foo stays";

        { // stress test: two concurrent changes
            repo::Sequence fork(master);
            const auto thorn = ops::particle::create<Foo>(fork, Foo::Quantum{integer{10}});
            {
                auto mod_a = ops::particle::modifier<Foo>(fork, thorn);
                auto mod_b = ops::particle::modifier<Foo>(fork, thorn);
                mod_a->data_field = integer{8};
                mod_b->data_field = integer{6};
            }
            EXPECT_EQ(debug::read<Foo>(fork, thorn)->data_field, integer{8}) << "Last-wins is the only merge policy for now";
        }

        { // Sequence: no validation until on_finish() into Branch
            const std::size_t before = debug::count<Foo>(master);
            repo::Sequence seq{master};
            for (int v = -5; v <= 5; ++v) {
                ops::particle::create<Foo>(seq, Foo::Quantum{integer{v}});
            }

            EXPECT_EQ(debug::count<Foo>(seq) - before, std::size_t{11}) << "Sequence as-is; no validation until on_finish()";

            seq.complete();
            EXPECT_EQ(debug::count<Foo>(master), before + std::size_t{11}) << "etalon validators are degenerate; all Foo's enter master";
        }

        { // Accumulator: no integrate/validate until on_finish() into Branch
            const std::size_t before = debug::count<Foo>(master);

            repo::Accumulator acc{master};
            const std::size_t head_count = debug::count<Foo>(acc);
            const std::size_t assumed_count = debug::count<Foo>(acc);
            const int computed = static_cast<int>(assumed_count) - static_cast<int>(head_count) - 1;

            for (int i = 0; i < 11; ++i) {
                ops::particle::create<Foo>(acc, Foo::Quantum{integer{computed}});
            }

            EXPECT_EQ(debug::count<Foo>(acc), head_count) << "Accumulator must not integrate head while accumulating";

            acc.complete();
            EXPECT_EQ(debug::count<Foo>(master), before + std::size_t{11}) << "etalon validators are degenerate; all accumulated Foo's enter master";
        }
    }
}

