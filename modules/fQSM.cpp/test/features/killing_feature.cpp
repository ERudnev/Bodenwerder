#include "_common.h"

#include <fQSM/api/interface.h>
#include <fQSM/state/world/data.h>


namespace {
    using namespace fqsm::api;
    namespace local {
        struct Body : Entity<Body> {
            struct Quantum {
                integer powerOfMass; // mass (kg) == 1 * 2^powerOfMass
            };
            struct Global {
                integer dustKgs = 0;// == mass (kg)
            };
            static const Codex codex;
            struct Actions : BaseActions {
                integer mass(Reading context, Id id) {
                    return 1 << get(context, id).powerOfMass;
                }
                void resetField(Writing context) {
                    //global
                }
            };
        };

        struct Life : Component<Life, Body> {
            struct Quantum {
                integer clock = 0;
            };
            static const Codex codex;
            struct Actions : BaseActions {
                static void create(Writing context, Body::Id id) {
                    ask::item::create<Life>(context, id, {0});
                }
                static Quantum update(const Quantum& q) {
                    return Quantum{q.clock+1};
                }
                static void update(Writing context, Id) {
                    context.reserve_broad_update<Life>();
                    for (const auto entry : context.state.items<Life>()) {
                        context.patch().template items<Life>().insert(entry.first, update(entry.second));
                    }
                }
            };
        };

        struct Death : Component<Death, Life> {
            struct Quantum {
                integer limit;
            };
            static const Codex codex;
            struct Actions : BaseActions {
                static void create(Writing context, Life::Id id) {
                    const auto body = ask::item::get<Body>(context, id);
                    ask::item::create<Death>(context, id, {body->powerOfMass + 1} ); // mass 1kg lives 1 sec
                }
                static void update(Writing context, Id) {
                    for (const auto entry : static_cast<Reading>(context).items<Death>()) {
                        if (with<Life>::get(context, entry.first).clock > entry.second.limit)
                            with<Life>::kill(context, entry.first);
                    }
                }
            };
        };
    }

    namespace local {
        const Body::Codex Body::codex = {};
        const Life::Codex Life::codex = {
            norma::component<Life, Body>(ComponentMissing::make_default, &Life::Actions::create),
        };
        const Death::Codex Death::codex = {
            norma::component<Death, Life>(ComponentMissing::make_default, &Death::Actions::create),
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

    fqsm::state::world::Data world(schema);
    context::Realm main(world);

    { // Single Stone kill:
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
        // create one Stone with
        // run as many updates as needed for first death/echo
        // run next updates for second
    }
}

} // namespace tests
