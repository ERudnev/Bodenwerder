#include <base/logging.h>
#include <base/testing/macros.h>

#include <fQSM/api/interface.h>

namespace {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            float x;
            float y;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };
}

int main() {
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
    });

    establish::Realm realm(schema);

    const auto id = with<A>::create(realm, {.x = 1.f, .y = 2.f});

    EXPECT_EQ(with<A>::get(realm, id).x, 1.f);
    EXPECT_EQ(with<A>::get(realm, id).y, 2.f);

    base::message("workshop: A ok");
    return 0;
}
