#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
using namespace fqsm::api;

namespace model {

    struct A : Entity<A> {
        struct Quantum {
            integer value = 0;
        };
        struct Global {
            integer deletions = 0;
        };
        struct Internals : DefaultInternals {
            static void countDeletion(Retrospecting context, Id, const Quantum&) {
                auto copy = with<A>::get_global(context);
                ++copy.deletions;
                context.workers_interface().updates<A>().put_global(copy);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::deletion<A>(&Internals::countDeletion),
            };
        }
    };
}
} // namespace

namespace tests {

void global_is_change_too()
{
    using namespace model;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
    });

    establish::Realm main(schema);

    const auto id = with<A>::create(main, {1});
    EXPECT_EQ(with<A>::get_global(main).deletions, 0);

    with<A>::remove(main, id);

    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(with<A>::exists(main, id));
    EXPECT_EQ(with<A>::get_global(main).deletions, 1)
        << "deletion reaction changed only Global; this must still count as a non-empty reaction wave";
}

} // namespace tests
