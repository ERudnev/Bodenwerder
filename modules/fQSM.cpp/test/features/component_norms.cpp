#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
    using namespace fqsm::api;
    namespace local {
        struct A : Entity<A> {
            struct Quantum { integer value; };
            static const Codex codex;
        };

        struct B : Component<B, A> {
            struct Quantum { string text; };
            static const Codex codex;
        };

        struct C : Component<C, A> {
            struct Quantum { integer power; };
            static const Codex codex;
            struct Actions : BaseActions {
                static void create(Writing context, A::Id id) {
                    ask::item::create<C>(context, id, {with<A>::get(context, id).value});
                }
            };
        };
    }

    // kinda impl in come *.cpp file:
    namespace local {
        const A::Codex A::codex = {};
    }
    namespace local {
        const B::Codex B::codex = {
            norma::component<B, A>(ComponentMissing::inacceptable),
            reaction::debug_death_event<B>("death-event message for {}"),
        };
    }
    namespace local {
        const C::Codex C::codex = {
            norma::component<C, A>(ComponentMissing::make_default, &C::Actions::create),
        };
    }
}

namespace tests {

void component_norms()
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

    { // Scenario 1: B::Codex::component<> aborted creation of A
        const auto id = ask::item::create<A>(main, {4});
        EXPECT_FALSE(ask::item::exists<A>(main, id));
        EXPECT_TRUE(main.notes().rejection());
    }
    { // Scenario 2: A+B manual, C via make_default; parent removal cascades to both components
        const auto id = [&] {
            context::Branch tx(main);
            const auto id = ask::item::create<A>(tx, {4});
            ask::item::create<B>(tx, id, {"manual"});
            return id;
        }();

        EXPECT_FALSE(main.notes().rejection());
        EXPECT_TRUE(ask::item::exists<A>(main, id));
        EXPECT_TRUE(ask::item::exists<B>(main, id));
        EXPECT_EQ(ask::item::get<C>(main, id)->power, 4);

        {
            context::Branch tx(main);
            ask::item::update<A>(tx, id).remove();
        }

        EXPECT_FALSE(main.notes().rejection());
        EXPECT_FALSE(ask::item::exists<A>(main, id));
        EXPECT_FALSE(ask::item::exists<B>(main, id));
        EXPECT_FALSE(ask::item::exists<C>(main, id));
    }
    // TDO: use this some day
    //EXPECT_EQ(ask::item::get<B>(main, id)->text, "generated");

}

} // namespace tests
