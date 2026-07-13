#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            int value = 0;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct B : Component<B, A> {
        struct Quantum {};
        struct Global {
            integer releaseCalls = 0;
            integer parentFound = 0;
            integer missingParent = 0;
            integer lastSeenParentValue = -1;
        };
        struct Internals : DefaultInternals {
            static void release(Retrospecting context, Id id, const Quantum&) {
                auto snapshot = with<B>::get_global(context);
                ++snapshot.releaseCalls;

                const bool parent_exists = with<A>::exists(context, id);
                if (parent_exists) {
                    const auto& parent = with<A>::get(context, id);
                    ++snapshot.parentFound;
                    snapshot.lastSeenParentValue = parent.value;
                } else {
                    ++snapshot.missingParent;
                }
                context.workers_interface().updates<B>().put_global(snapshot);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::deletion<B>(&Internals::release),
            };
        }
    };

    struct Composition : Archetype<Composition> {
        static A::Id spawn(Writing context, int value) {
            const auto id = with<A>::create(context, {.value = value});
            with<B>::extend(context, id, {});
            return id;
        }
    };
}
} // namespace

namespace tests {

void destructor_like_reactions()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
    });

    establish::Realm main(schema);

    const auto id = with<Composition>::spawn(main, 42);

    with<A>::remove(main, id);

    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(with<A>::exists(main, id)) << "parent A must be removed";
    EXPECT_FALSE(with<B>::exists(main, id)) << "component B must be removed together with parent A";

    const auto& global = with<B>::get_global(main);
    EXPECT_EQ(global.releaseCalls, 1) << "B::release must run exactly once";
    EXPECT_EQ(global.parentFound, 1) << "retrospective destructor must still see parent A";
    EXPECT_EQ(global.missingParent, 0) << "parent A must not disappear from retrospective view";
    EXPECT_EQ(global.lastSeenParentValue, 42) << "retrospective destructor must read parent's last stable value";
}

} // namespace tests
