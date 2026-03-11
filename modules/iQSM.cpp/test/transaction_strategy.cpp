#include <base/testing/macros.h>

#include <algorithm>

#include <Atomic/varph.q1.h>
#include <iQSM/_all.include.h>

namespace tests {

    using namespace iqsm::dsl_gateway;

    struct Foo : Entity<Foo>, Require<> {
        struct Quantum {
            integer value;
        };
        static const Invariants invariants;
    };

    namespace Foo_impl {
        static auto confine_value_one(World, Foo::Id, const Foo::Quantum& original) -> optional<Foo::Quantum> {
            auto updated = original;
            updated.value = std::clamp(updated.value, integer{0}, integer{10});
            if (Facet<Foo>::equal(original, updated)) return {};
            return updated;
        }

        static Delta confine_value(World world) {
            return iqsm::ops::cache::update<Foo, &Foo_impl::confine_value_one>(world);
        }
    }

    const Invariants Foo::invariants{{{
        &Foo_impl::confine_value,
    }}};


    struct Bar : Component<Bar, Foo>, Require<Foo> {
        struct Quantum {
            integer factor = 0;
        };
        static const Invariants invariants;
    };

    namespace Bar_impl {
        static integer factorial(integer n) {
            integer out = 1;
            for (integer i = 2; i <= n; ++i) out *= i;
            return out;
        }

        static auto construct(World, const Foo::Quantum& foo) -> Bar::Quantum {
            return Bar::Quantum{.factor = factorial(foo.value)};
        }

        static auto update_cache_one(World world, Bar::Id id, const Bar::Quantum& original) -> optional<Bar::Quantum> {
            const auto required = factorial(iqsm::ops::particle::get<Foo>(world, id).value);
            if (original.factor == required) return {};
            return Bar::Quantum{.factor = required};
        }

        static Delta update_cache(World world) {
            return iqsm::ops::cache::update<Bar, &Bar_impl::update_cache_one>(world);
        }
    }

    const Invariants Bar::invariants{{{
        Invariants::anchor_component<Foo, Bar, &Bar_impl::construct>,
        &Bar_impl::update_cache,
    }}};
}

namespace tests {
    void transaction_strategy() {
        using namespace iqsm;
        using namespace iqsm::dsl_gateway;

        World world = ops::world::create(ops::schema::assemble<Foo, Bar>());

        {
            ops::Transaction tx{World{world}, ops::Transaction::Policy::accumulator};

            const auto addFoo = [&](integer value) -> Foo::Id {
                const auto id = Foo::Id::generate_random();

                auto fd = base::make_shared<delta::FieldDiff<Foo>>();
                auto op = delta::FieldDiff<Foo>::Operation{};
                op.add = Facet<Foo>::create(Foo::Quantum{value});
                fd->ops = fd->ops.insert(id, std::move(op));

                auto wd = base::make_shared<delta::Fields>();
                wd->fields = wd->fields.insert(
                    Facet<Foo>::typeId,
                    freeze(fd));

                tx.absorb(freeze(wd));
                return id;
            };

            const auto f0 = addFoo(integer{-5});
            const auto f1 = addFoo(integer{0});
            const auto f2 = addFoo(integer{5});
            const auto f3 = addFoo(integer{10});
            const auto f4 = addFoo(integer{15});

            tx.flush();
            world = tx.current;

            world = ops::integrate_raw(world, Foo::invariants.apply(world));
            world = ops::integrate_raw(world, Bar::invariants.apply(world));

            EXPECT_EQ(world->field<Bar>()->container.size(), size_t{5});
            EXPECT_EQ(ops::particle::get<Foo>(world, f0).value, integer{0});
            EXPECT_EQ(ops::particle::get<Foo>(world, f4).value, integer{10});
            EXPECT_EQ(ops::particle::get<Bar>(world, f0).factor, Bar_impl::factorial(integer{0}));
        }
    }
}

