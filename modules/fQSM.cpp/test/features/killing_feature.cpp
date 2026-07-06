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
                static integer mass(Reading context, Id id) {
                    return 1 << get(context, id).powerOfMass;
                }
            };
            struct Internals;
            static const Behavior customAspectReactions();
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
                    create_for(context, id, {0});
                }

                static void update(Writing context, int timePassed) {
                    for (const auto entry : context->aspect<Life>().items()) {
                        modify(context, entry.id)->clock += timePassed;
                    }
                }
            };
            struct Internals : DefaultInternals{};
            static const Behavior customAspectReactions() { return {}; }
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
                    create_for(context, id, {body.powerOfMass + 1}); // mass 1kg lives 1 sec
                }
            };
            struct Internals : DefaultInternals{};
            static const Behavior customAspectReactions() { return {}; }
        };
    }

    namespace local::archetype {
        struct Stone : Archetype<Stone> {
            static Body::Id spawn(Writing context, int powerOfMass) {
                const auto id = with<Body>::create(context, {powerOfMass});
                with<Life>::create(context, id);
                with<Death>::create(context, id);
                return id;
            }
        };
    }

    // this kind of code may appear in the separate *.cpp
    namespace local {
        struct Body::Internals : DefaultInternals {
            // this reaction is private (as any reaction) because it must not be called manually
            static void reactOnDeath(Writing context, Id id, const Quantum& lastValue) {
                // simple create 2 lesser stones:
                with<archetype::Stone>::spawn(context, lastValue.powerOfMass - 1);
                with<archetype::Stone>::spawn(context, lastValue.powerOfMass - 1);
            }
        };

        auto Body::customAspectReactions() -> const Behavior {
            return {
                reaction::deletion<Body>(&Internals::reactOnDeath),
            };
        }
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

    { // Single Stone kraken:
        context::Realm main(schema);
        const auto id = with<archetype::Stone>::spawn(main, 4);

        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(with<Body>::exists(main, id));

        {
            context::Branch branch(main);
            with<Death>::kraken(branch, id);

            EXPECT_TRUE(main.result().good());
            EXPECT_TRUE(with<Body>::exists(main, id)) << "removed in still not commited Branch";
        }
        EXPECT_TRUE(main.result().good());
        EXPECT_FALSE(with<Body>::exists(main, id)) << "removed from world after Branch commit";
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
