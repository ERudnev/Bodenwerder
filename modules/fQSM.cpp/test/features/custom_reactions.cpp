#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
    using namespace fqsm::api;
    namespace local {
        struct A : Entity<A> {
            struct Quantum { integer value; };
            struct Reactions : BaseReactions {
                static const Behavior custom;
            };
        };

        struct B : Component<B, A> {
            struct Quantum { string text; };
            struct Reactions : BaseReactions {
                static const Behavior custom;
            };
        };

        struct C : Component<C, A> {
            struct Quantum { integer power; };
            struct Actions : BaseActions {
                static void create(Writing context, A::Id id) {
                    create_for(context, id, {with<A>::get(context, id).value});
                }
            };
            struct Reactions : BaseReactions {
                static const Behavior custom;
            };
        };
    }

    namespace local::archetype {
        struct EntABC : Archetype<EntABC> {
            static A::Id spawn(Writing context, int val) {
                const auto id = with<A>::create(context, {val});
                with<B>::create_for(context, id, {"manual"});
                with<C>::create(context, id);
                return id;
            }
        };
    }

    // kinda impl in come *.cpp file:
    namespace local {
        const A::Reactions::Behavior A::Reactions::custom = {};
        const B::Reactions::Behavior B::Reactions::custom = {
            reaction::debug::death_log<B>("death-event message for {}"),
        };
        const C::Reactions::Behavior C::Reactions::custom = {};
    }
}

namespace tests {

void custom_reactions()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
        ask::schema::aspect<C>(),
    });

    fqsm::model::complex::Reality world(schema);
    context::Realm main(world);

    { // Scenario 1: B::Behavior::component<> aborted creation of A
        const auto id = with<A>::create(main, {4});
        EXPECT_FALSE(with<A>::exists(main, id));
        EXPECT_FALSE(main.result().good());
    }
    { // Scenario 2: A+B manual, C via make_default; parent removal cascades to both components
        const auto id = [&] {
            context::Branch tx(main);
            return with<archetype::EntABC>::spawn(tx, 4);
        }();

        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(with<A>::exists(main, id));
        EXPECT_TRUE(with<B>::exists(main, id));
        EXPECT_EQ(with<C>::get(main, id).power, 4);

        {
            context::Branch tx(main);
            with<A>::remove(tx, id);
        }

        EXPECT_TRUE(main.result().good());
        EXPECT_FALSE(with<A>::exists(main, id));
        EXPECT_FALSE(with<B>::exists(main, id));
        EXPECT_FALSE(with<C>::exists(main, id));

        //EXPECT_FALSE(true) << "feature is incompleted: separate Behavior as base class";
    }
    // TDO: use this some day
    //EXPECT_EQ(with<B>::get(main, id).text, "generated");

}

} // namespace tests
