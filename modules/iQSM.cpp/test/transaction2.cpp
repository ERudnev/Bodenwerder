#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>
#include <iQSM/operations/transaction2.h>

namespace tests {
    using namespace iqsm::dsl_gateway;

    namespace {
        struct Foo : Entity<Foo>, Require<> {
            struct Quantum {
                integer value = 0;
            };
            static const Invariants invariants;
        };

        struct Bar : Attribute<Bar, Foo>, Require<Foo> {
            struct Quantum {};
            static const Invariants invariants;
        };

        namespace Foo_impl {
            static iqsm::Delta validate_nonnegative(iqsm::World world) {
                const auto field = world->field<Foo>();

                auto fd = base::make_shared<iqsm::delta::FieldDiff<Foo>>();

                for (const auto& [id, item] : field->container) {
                    if (item->value >= integer{0}) continue;
                    typename iqsm::delta::FieldDiff<Foo>::Operation op{};
                    op.remove = true;
                    fd->ops = fd->ops.insert(id, std::move(op));
                }

                if (fd->ops.empty()) return iqsm::delta::empty();

                auto wd = base::make_shared<iqsm::delta::Fields>();
                wd->fields = wd->fields.insert(iqsm::Facet<Foo>::typeId, iqsm::freeze(fd));
                return iqsm::freeze(wd);
            }
        }

        const Invariants Foo::invariants{{{
            &Foo_impl::validate_nonnegative,
        }}};

        const Invariants Bar::invariants{{{
            Invariants::anchor_attribute<Foo, Bar>,
        }}};
    } // anonymous namespace

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
        auto nested = iqsm::ops::Transaction2::sync(gate.pre_cursor());

        const auto created = create_foo_operator(nested.pre_cursor(), 1);
        nested.absorb(created.delta);
        nested.absorb(add_bar_if_odd_operator(nested.pre_cursor(), created.receipt));

        gate.absorb(nested.finish());
        out_created.push_back(created.receipt);
    }

    void transaction2() {
        using namespace iqsm;

        World master = ops::world::create(ops::schema::assemble<Foo, Bar>());

        auto adding_first_objects = ops::Transaction2::accumulate(master);
        std::vector<Foo::Id> first_objects;
        for (int xx = 0; xx < 10; ++xx )
        {
            const auto created = create_foo_operator(adding_first_objects.pre_cursor(), xx );
            first_objects.push_back(created.receipt);
            adding_first_objects.absorb(created.delta);
        }
        master = ops::validate(ops::integrate(master, adding_first_objects.finish()));

        // Sequential (dependent) changes: `pre_cursor()` is only a working/sync view.
        // It must not be treated as a "result world" in any mode.
        auto sequential = ops::Transaction2::sync(master);
        const auto created = create_foo_operator(sequential.pre_cursor(), 1);
        sequential.absorb(created.delta);
        sequential.absorb(add_bar_if_odd_operator(sequential.pre_cursor(), created.receipt));

        const auto after = ops::validate(ops::integrate(master, sequential.finish()));
        EXPECT_TRUE(ops::particle::exists<Bar>(after, created.receipt));

        // Nested transactions: outer accumulates, inner integrates for dependent steps.
        auto outer = ops::Transaction2::accumulate(master);
        std::vector<Foo::Id> nested_ids;
        analytics_worker(outer, nested_ids); // implicit Transaction2 -> Context2
        const auto after_nested = ops::validate(ops::integrate(master, outer.finish()));
        EXPECT_TRUE(ops::particle::exists<Bar>(after_nested, nested_ids.at(0)));

        // Using `pre_cursor()` as a "result world" is wrong in any mode.
        {
            auto tx = ops::Transaction2::accumulate(master);
            const auto made = create_foo_operator(tx.pre_cursor(), 7);
            tx.absorb(made.delta);

            const auto delta = tx.finish();
            const auto applied = ops::validate(ops::integrate(master, delta));

            const auto exists = [&](World w) { return ops::particle::exists<Foo>(w, made.receipt); };
            EXPECT_TRUE(exists(applied));
            EXPECT_TRUE(not exists(tx.pre_cursor()));
        }
    }
}
