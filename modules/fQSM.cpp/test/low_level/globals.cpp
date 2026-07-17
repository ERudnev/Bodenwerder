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
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
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

    establish::Realm main(schema);

    with<A>::modify_global(main)->globalValue = 2;
    EXPECT_EQ(with<A>::get_global(main).globalValue, 2) << "unnamed GlobalGate: Writing collapsed into realm at end of full-expression";

    {
        auto tx = with<A>::modify_global(main);
        tx->globalValue = 7;
        // get_global(main) reads committed realm, not the still-open Writing patch
        EXPECT_EQ(with<A>::get_global(main).globalValue, 2) << "named GlobalGate: realm not updated until Writing collapses";
    }

    EXPECT_EQ(with<A>::get_global(main).globalValue, 7) << "after GlobalGate destroyed, Writing collapsed into realm";

    main.branch([&](Writing context) {
        with<A>::modify_global(context)->globalValue = 11;
        EXPECT_EQ(with<A>::get_global(context).globalValue, 11) << "same Writing: Future sees GlobalGate patchlet immediately";
    });
    EXPECT_EQ(with<A>::get_global(main).globalValue, 11);
}

} // namespace tests
