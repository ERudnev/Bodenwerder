#include "_common.h"

#include <fQSM/api/interface.h>

using namespace fqsm::api;

namespace model {

    struct A : Entity<A> {
        struct Quantum {};
        struct Global {
            int globalValue{};
        };
        using Reactions = DefaultReactions;
    };
}

namespace tests {

void globals()
{
    using namespace model;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
    });

    context::Realm main(schema);

    auto tx = ask::global::update<A>(main)->globalValue = 2;
    EXPECT_EQ(ask::global::get<A>(main).globalValue, 2);

    {
        auto tx = ask::global::update<A>(main);
        tx->globalValue = 7;
    }

    EXPECT_EQ(ask::global::get<A>(main).globalValue, 7);
}

} // namespace tests
