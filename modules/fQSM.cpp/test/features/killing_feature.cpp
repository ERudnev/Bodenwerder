#include "_common.h"

#include <fQSM/api/interface.h>
#include <fQSM/state/world/data.h>

namespace {
    using namespace fqsm::api;
    namespace local {
        struct A : Entity<A> {
            struct Quantum { integer value; };
            static const Codex codex;
        };

        struct B : Component<B, A> {
            struct Quantum { string text; };
            static const Codex codex;
            struct Actions : BaseActions {};
        };

        struct C : Component<C, B> {
            struct Quantum { integer power; };
            static const Codex codex;
            struct Actions : BaseActions {};
        };
    }

    namespace local {
        const A::Codex A::codex = {};
        const B::Codex B::codex = {};
        const C::Codex C::codex = {};
    }
}

namespace tests {

void killing_feature()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
        ask::schema::aspect<C>(),
    });

    fqsm::state::world::Data world(schema);
    context::Realm main(world);

    const auto id = [&] {
        context::Branch tx(main);
        const auto id = ask::item::create<A>(tx, {4});
        ask::item::create<B>(tx, id, {"b"});
        ask::item::create<C>(tx, id, {8});
        return id;
    }();

    EXPECT_FALSE(main.notes().rejection());
    EXPECT_TRUE(ask::item::exists<A>(main, id));

    C::Actions::kill(main, id);

    EXPECT_FALSE(main.notes().rejection());
    EXPECT_FALSE(ask::item::exists<A>(main, id));
}

} // namespace tests
