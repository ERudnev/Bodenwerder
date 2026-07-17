#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            integer value;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };
}

namespace tests {

void quantal()
{
    using namespace fqsm::api;

    const Schema schema = ask::schema::aspect<A>();
    establish::Realm main(schema);

    main.branch([&](Writing context) {
        const auto id = with<A>::create(context, {.value = 0});

        {
            auto gate = with<A>::modify(context, id);
            gate->value = 42;
            // same Writing: get sees patchlet immediately (no private Quantal buffer)
            EXPECT_EQ(with<A>::get(context, id).value, 42);
        }

        EXPECT_EQ(with<A>::get(context, id).value, 42);
    });
}

} // namespace tests
