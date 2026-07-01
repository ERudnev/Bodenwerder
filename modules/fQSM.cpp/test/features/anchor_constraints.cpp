#include "_common.h"

#include <fQSM/api/interface.h>

namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {};
        using Reactions = DefaultReactions;
    };

    struct B : Entity<B> {
        struct Quantum {
            A::Id anchor;
        };
        using Reactions = DefaultReactions;
    };
}

namespace tests {

void anchor_constraints()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
    });

    context::Realm main(schema);

    const auto aId = ask::item::create<A>(main, {});
    const auto bId = ask::item::create<B>(main, {.anchor = aId});

    ask::item::update<A>(main, aId).remove();

    EXPECT_FALSE(ask::item::exists<B>(main, bId)) << "B must die when anchored A is removed";
}

} // namespace tests
