#include "_common.h"

#include <Atomic/varph.q1.h>
#include <iQSM/operations/transaction2.h>

#include "utilities/fooBar.h"

namespace tests {
    using utility::Bar;
    using utility::Foo;

    // Two kinds of "world-changing" logic blocks:
    // - worker: takes Context, returns void; reports changes via Context (into an outer Transaction)
    // - operator: takes World (and optional Id/params), returns Delta / DeltaAnd<...>
    static auto dec_1_Foo_operator(iqsm::World world, Foo::Id id) -> iqsm::Delta {
        const auto before = iqsm::ops::particle::item<Foo>(world, id);
        auto updated = *before;
        updated.value -= integer{1};

        auto fd = base::make_shared<iqsm::delta::FieldDiff<Foo>>();
        typename iqsm::delta::FieldDiff<Foo>::Operation op{};
        op.change = std::pair<typename iqsm::delta::FieldDiff<Foo>::Item, typename iqsm::delta::FieldDiff<Foo>::Item>{
            before,
            iqsm::Facet<Foo>::create(std::move(updated)),
        };
        fd->ops = fd->ops.insert(id, std::move(op));

        auto wd = base::make_shared<iqsm::delta::Fields>();
        wd->fields = wd->fields.insert(iqsm::Facet<Foo>::typeId, iqsm::freeze(fd));

        return iqsm::freeze(wd);
    }

    static auto create_foo_operator(iqsm::World, int initial_value) -> iqsm::DeltaAnd<Foo::Id> {
        const auto id = Foo::Id::generate_random();

        auto fd = base::make_shared<iqsm::delta::FieldDiff<Foo>>();
        typename iqsm::delta::FieldDiff<Foo>::Operation op{};
        op.add = iqsm::Facet<Foo>::create(Foo::Quantum{.value = integer{initial_value}});
        fd->ops = fd->ops.insert(id, std::move(op));

        auto wd = base::make_shared<iqsm::delta::Fields>();
        wd->fields = wd->fields.insert(iqsm::Facet<Foo>::typeId, iqsm::freeze(fd));

        return iqsm::DeltaAnd<Foo::Id>{iqsm::freeze(wd), id};
    }

    static auto add_bar_if_odd_operator(iqsm::World world, Foo::Id id) -> iqsm::Delta {
        if (not iqsm::ops::particle::exists<Foo>(world, id)) { return iqsm::delta::empty(); }
        if ((iqsm::ops::particle::get<Foo>(world, id).value % integer{2}) == integer{0}) { return iqsm::delta::empty(); }

        auto fd = base::make_shared<iqsm::delta::FieldDiff<Bar>>();
        typename iqsm::delta::FieldDiff<Bar>::Operation op{};
        op.add = iqsm::Facet<Bar>::create(Bar::Quantum{});
        fd->ops = fd->ops.insert(id, std::move(op));

        auto wd = base::make_shared<iqsm::delta::Fields>();
        wd->fields = wd->fields.insert(iqsm::Facet<Bar>::typeId, iqsm::freeze(fd));
        return iqsm::freeze(wd);
    }

    static void analytics_worker(iqsm::ops::Context2 gate, std::vector<Foo::Id>& out_created) {
        // Draft: a "big worker" may create a local transaction, then absorb its result upstream.
        auto nested = iqsm::ops::Transaction2::integrate(gate.current());

        const auto created = create_foo_operator(nested.current(), 1);
        nested.absorb(created.delta);
        nested.absorb(add_bar_if_odd_operator(nested.current(), created.receipt));

        gate.absorb(nested.finish());
        out_created.push_back(created.receipt);
    }

    void transaction2() {
        using namespace iqsm;

        auto master = ops::Holder{ops::world::create(ops::schema::assemble<Foo, Bar>())};

        auto adding_first_objects = ops::Transaction2::accumulate(master.current());
        std::vector<Foo::Id> first_objects;
        for (int xx = 0; xx < 10; ++xx )
        {
            const auto created = create_foo_operator(adding_first_objects.current(), xx );
            first_objects.push_back(created.receipt);
            adding_first_objects.absorb(created.delta);
        }
        master.absorb(adding_first_objects.finish());

        // Sequential (dependent) changes: `current()` is only a working/sync view.
        // It must not be treated as a "result world" in any mode.
        auto sequential = ops::Transaction2::integrate(master.current());
        const auto created = create_foo_operator(sequential.current(), 1);
        sequential.absorb(created.delta);
        sequential.absorb(add_bar_if_odd_operator(sequential.current(), created.receipt));

        const auto after = ops::validate(ops::integrate(master.current(), sequential.finish()));
        EXPECT_TRUE(ops::particle::exists<Bar>(after, created.receipt));

        // Nested transactions: outer accumulates, inner integrates for dependent steps.
        auto outer = ops::Transaction2::accumulate(master.current());
        std::vector<Foo::Id> nested_ids;
        analytics_worker(outer, nested_ids); // implicit Transaction2 -> Context2
        const auto after_nested = ops::validate(ops::integrate(master.current(), outer.finish()));
        EXPECT_TRUE(ops::particle::exists<Bar>(after_nested, nested_ids.at(0)));

        // Using `current()` as a "result world" is wrong in any mode.
        {
            auto tx = ops::Transaction2::accumulate(master.current());
            const auto made = create_foo_operator(tx.current(), 7);
            tx.absorb(made.delta);

            const auto delta = tx.finish();
            const auto applied = ops::validate(ops::integrate(master.current(), delta));

            const auto exists = [&](World w) { return ops::particle::exists<Foo>(w, made.receipt); };
            EXPECT_TRUE(exists(applied));
            EXPECT_TRUE(not exists(tx.current()));
        }
    }
}
