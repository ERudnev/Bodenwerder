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
        struct Actions : BaseActions {
            static void countDeletion(Writing context, Id, const Quantum&) {
                auto copy = global(context);
                ++copy.deletions;
                global_set(context, copy);
            }
        };
        struct Reactions : BaseReactions {
            inline static const Behavior custom = {
                reaction::deletion<A>(&Actions::countDeletion),
            };
        };
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

    context::Realm main(schema);

    const auto id = ask::item::create<A>(main, {1});
    EXPECT_EQ(with<A>::global(main).deletions, 0);

    ask::item::update<A>(main, id).remove();

    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(ask::item::exists<A>(main, id));
    EXPECT_EQ(with<A>::global(main).deletions, 1)
        << "deletion reaction changed only Global; this must still count as a non-empty reaction wave";
}

} // namespace tests
