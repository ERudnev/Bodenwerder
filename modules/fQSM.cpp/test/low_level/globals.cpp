#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
using namespace fqsm::api;

namespace model {

    struct A : Entity<A> {
        struct Quantum {};
        struct Global {
            int globalValue{};
        };
        // REWORK: using Reactions = DefaultReactions;
    };
}
} // namespace

namespace tests {

void globals()
{
    using namespace model;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
    });

    context::Realm main(schema);

    with<A>::modify_global(main)->globalValue = 2;
    EXPECT_EQ(with<A>::get_global(main).globalValue, 2) << "update commited immediately (unnamed RAII already dead)";

    {
        auto tx = with<A>::modify_global(main);
        tx->globalValue = 7;
        EXPECT_EQ(with<A>::get_global(main).globalValue, 2) << "update is postponed till the end of the scope";
    }

    EXPECT_EQ(with<A>::get_global(main).globalValue, 7) << "update comitted after the scope";
}

} // namespace tests
