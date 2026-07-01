#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
    using namespace fqsm::api;

    // this kind of code may appear in the separate *.h header file
    namespace local {
        struct Body : Entity<Body> {
            struct Quantum {
                integer powerOfMass; // mass (kg) == 1 * 2^powerOfMass
            };
            struct Global {
                integer dustKgs = 0;// == mass (kg)
            };
            struct Actions : BaseActions {
                struct Private; // important forward to allow Protave part defined in *.cpp file
                static integer mass(Reading context, Id id) {
                    return 1 << get(context, id).powerOfMass;
                }
            };
            struct Reactions : BaseReactions {
                static const Behavior custom;
            };
        };
    }

    // this kind of code may appear in the separate *.h header file
    namespace local {
        struct Life : Component<Life, Body> {
            struct Quantum {
                integer clock = 0;
            };
            struct Actions : BaseActions {
                static void create(Writing context, Body::Id id) {
                    new_element(context, id, {0});
                }
                static Quantum update(const Quantum& q, int timePassed) {
                    return Quantum{q.clock+timePassed};
                }
                static void update(Writing context, int timePassed) {
                    for (const auto entry : context->aspect<Life>().items()) {
                        //_INCOMPLETE_; // NB to mage Gate == Draft!
                        context.patch().aspect<Life>().items.insert(entry.id, update(entry.value, timePassed));
                    }
                }
            };
            struct Reactions : BaseReactions {
                static const Behavior custom;
            };
        };
    }

    // this kind of code may appear in the separate *.h header file
    namespace local {
        struct Death : Component<Death, Life> {
            struct Quantum {
                integer limit;
            };
            struct Actions : BaseActions {
                struct Private;
                static void create(Writing context, Life::Id id) {
                    const auto& body = with<Body>::get(context, id);
                    new_element(context, id, {body.powerOfMass + 1}); // mass 1kg lives 1 sec
                }
            };
            struct Reactions : BaseReactions {
                static const Behavior custom;
            };
        };
    }

    namespace local::archetype {
        struct Stone : Archetype {
            struct Actions : BaseActions {
                static Body::Id spawn(Writing context, int powerOfMass) {
                    const auto id = ask::item::create<Body>(context, {powerOfMass});
                    with<Life>::create(context, id);
                    with<Death>::create(context, id);
                    return id;
                }
            };
        };
    }

    // this kind of code may appear in the separate *.cpp
    namespace local {
        // Private part of Actions
        struct Body::Actions::Private {
            // this reaction id private (as any reaction) because it must not be called manually
            static void reactOnDeath(Writing context, Id id, const Quantum& lastValue) {
                // simple create 2 lesser stones:
                with<archetype::Stone>::spawn(context, lastValue.powerOfMass - 1);
                with<archetype::Stone>::spawn(context, lastValue.powerOfMass - 1);
            }
        };

        const Body::Reactions::Behavior Body::Reactions::custom = {
            reaction::deletion<Body>(&Actions::Private::reactOnDeath),
        };
        const Life::Reactions::Behavior Life::Reactions::custom = {};
        const Death::Reactions::Behavior Death::Reactions::custom = {};
    }

    // this kind of code may appear in the separate *.cpp
    namespace local {
        struct Death::Actions::Private {
            static void reactOnParentUpdates(fqsm::Reacting context, HostAspect::Id id, const HostAspect::Quantum& newState) {
            }
        };
    }
}

namespace tests {

void killing_feature()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Body>(),
        ask::schema::aspect<Life>(),
        ask::schema::aspect<Death>(),
    });

    { // Single Stone kill:
        context::Realm main(schema);
        const auto id = with<archetype::Stone>::spawn(main, 4);

        EXPECT_FALSE(main.notes().rejection());
        EXPECT_TRUE(ask::item::exists<Body>(main, id));

        {
            context::Branch branch(main);
            with<Death>::kill(branch, id);

            EXPECT_FALSE(main.notes().rejection());
            EXPECT_TRUE(ask::item::exists<Body>(main, id)) << "removed in still not commited Branch";
        }
        EXPECT_FALSE(main.notes().rejection());
        EXPECT_FALSE(ask::item::exists<Body>(main, id)) << "removed from world after Branch commit";
    }
    { // Lifetime simulation
        context::Realm main(schema);

        const auto id = with<archetype::Stone>::spawn(main, 4);
        for (int xx = 0; xx < 100; ++xx)
            with<Life>::update(main, 1);
        //EXPECT_EQ()
        // run as many updates as needed for first death/echo
        // run next updates for second
    }
}

} // namespace tests
