#include "_common.h"

#include <fQSM/api/interface.h>

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
                static integer mass(Reading context, Id id) {
                    return 1 << get(context, id).powerOfMass;
                }
                static void resetField(Writing context) {
                    ask::global::update<Own>(context)->dustKgs = 0;
                    //global(context).dustKgs = 0;
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
                    context.expect_broad_update<Life>();
                    for (const auto entry : context->aspect<Life>().items()) {
                        _INCOMPLETE_; // NB to mage Gate == Draft!
                        context.patch()->aspect<Life>().items.insert(entry.id, update(entry.value));
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
                    for (const auto entry : context->aspect<Death>().items()) {
                        if (with<Life>::get(context, entry.id).clock > entry.value.limit)
                            with<Life>::kill(context, entry.id);
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

    context::Realm main(schema);

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
        with<Body>::resetField(main);
        ask::item::create<Body>(main, {4});
        //for (int xx = 0; xx < 100; ++xx)
        //    with<Life>::update(main, 1);
        //EXPECT_EQ()
        // run as many updates as needed for first death/echo
        // run next updates for second
    }
}

} // namespace tests
