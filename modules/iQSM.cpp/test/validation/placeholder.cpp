#include "../_common.h"

namespace {
    using namespace iqsm::dsl_gateway;

    struct Foo : Entity<Foo>, Require<> {
        struct Quantum { integer value = 0; };
        struct Global {};
        static const Invariants invariants;
        struct Operations : OwnTypeOperations {};
    };

    struct Bar : Component<Bar, Foo>, Require<Foo> {
        struct Quantum {};
        struct Global {};
        static const Invariants invariants;
        struct Operations : OwnTypeOperations {
            static void remove(Writing commit, Id id) { ops::particle::remove<Foo>(commit, id); }
        };
    };

    const Invariants Foo::invariants{};

    struct Bar_private : Bar::Operations {
        static auto construct(Reading, Id, Item<Foo>) -> Quantum { return {}; }
    };

    const Invariants Bar::invariants{
        .structural = {
            invariant::isomorphic<Foo, Bar, &Bar_private::construct>,
        },
        .logical = {},
    };
}

namespace tests {
    void validation_placeholder() {
        const auto empty = ops::world::create(ops::schema::assemble<Foo, Bar>());
        repo::Branch master{empty};

        const auto foo_id = ops::particle::create<Foo>(master, Foo::Quantum{.value = 1});
        EXPECT_TRUE(ops::particle::exists<Bar>(master, foo_id));

        Bar::Operations::remove(master, foo_id);
        EXPECT_TRUE(not ops::particle::exists<Foo>(master, foo_id));
        EXPECT_TRUE(not ops::particle::exists<Bar>(master, foo_id));
    }
}

