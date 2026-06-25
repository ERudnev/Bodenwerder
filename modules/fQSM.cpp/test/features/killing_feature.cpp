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
            static const Behavior custom;
            struct Actions : BaseActions {
                struct Private; // important forward to allow Protave part defined in *.cpp file
                static integer mass(Reading context, Id id) {
                    return 1 << get(context, id).powerOfMass;
                }
            };
        };
    }

    // this kind of code may appear in the separate *.h header file
    namespace local {
        struct Life : Component<Life, Body> {
            struct Quantum {
                integer clock = 0;
            };
            static const Behavior custom;
            struct Actions : BaseActions {
                static void create(Writing context, Body::Id id) {
                    ask::item::create<Life>(context, id, {0});
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
        };
    }

    // this kind of code may appear in the separate *.h header file
    namespace local {
        struct Death : Component<Death, Life> {
            struct Quantum {
                integer limit;
            };
            static const Behavior custom;
            struct Actions : BaseActions {
                struct Private;
                static void create(Writing context, Life::Id id) {
                    const auto body = ask::item::get<Body>(context, id);
                    ask::item::create<Death>(context, id, {body->powerOfMass + 1} ); // mass 1kg lives 1 sec
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
                ask::item::create<Body>(context, {lastValue.powerOfMass - 1});
                ask::item::create<Body>(context, {lastValue.powerOfMass - 1});
            }
        };

        // Behavior:
        const Body::Behavior Body::custom = {
            reaction::deletion<Body>(&Actions::Private::reactOnDeath),
        };
    }

    // this kind of code may appear in the separate *.cpp
    namespace local {
        const Life::Behavior Life::custom = {
            rule::structural_deprecated::component<Life, Body>(reflex::ComponentMissing::make_default, &Life::Actions::create),
        };
    }

    // this kind of code may appear in the separate *.cpp
    namespace local {
        struct Death::Actions::Private {
            static void reactOnParentUpdates(fqsm::Reviewing context, HostAspect::Id id, const HostAspect::Quantum& newState) {
            }
        };
        const Death::Behavior Death::custom = {
            rule::structural_deprecated::component<Death, Life>(reflex::ComponentMissing::make_default, &Death::Actions::create),
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

        const auto id = [&] {
            context::Branch tx(main);
            // this is valid case to build Entity when Item value normalization is present:
            const auto id = ask::item::create<Body>(tx, {4});
            ask::item::create<Life>(tx, id, {0});
            ask::item::create<Death>(tx, id, {10000});
            return id;
        }();

        EXPECT_FALSE(main.notes().rejection());
        EXPECT_TRUE(ask::item::exists<Body>(main, id));

        Death::Actions::kill(main, id);

        EXPECT_FALSE(main.notes().rejection());
        EXPECT_FALSE(ask::item::exists<Body>(main, id));
    }
    { // Lifetime simulation
        context::Realm main(schema);

        ask::item::create<Body>(main, {4});
        for (int xx = 0; xx < 100; ++xx)
            with<Life>::update(main, 1);
        //EXPECT_EQ()
        // run as many updates as needed for first death/echo
        // run next updates for second
    }
}

} // namespace tests
