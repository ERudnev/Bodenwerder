#include "_common.h"

#include <fQSM/api/interface.h>

// kinda header:
namespace {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            integer value;
        };
        static const Behavior behavior;
        struct Actions : BaseActions {
            struct Private;
            static void fastJob(Direct<A> context, int bonus) {
                for (auto [_, item] : context.items)
                    item.value += bonus;
            }
            static void slowJob(Writing context, int bonus) {
                auto& items = context->aspect<A>().items();
                for (auto [id, item] : items)
                    ask::item::update<A>(context, id)->value += bonus;
            }
            /* TODO: this is other story, "modern" access to patch through Draft interface (needs reworked Gate)
            static void modernJob(ModernGate context, int bonus) {
                for (auto [_, item] : context->aspect<A>().items())
                    item.value += bonus;
            }*/
        };
    };
}

// kinda CPP part:
namespace {
    using namespace fqsm::api;

    struct A::Actions::Private : A::Actions {
        static auto allow_non_negative(const Quantum& data) -> Update {
            if (data.value >= 0) return std::nullopt;
            return Quantum{.value = 0};
        }
    };

    const A::Behavior A::behavior = {
        rule::constraint::value_X<A>(&A::Actions::Private::allow_non_negative),
    };
}

// test itself:
namespace tests {

void immediate()
{
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
    });

    context::Realm main(schema);

    std::vector<A::Id> ids;
    {
        context::Branch local(main);
        for (int xx = 0; xx < 10; ++xx)
            ids.push_back(ask::item::create<A>(local, {xx}));
    }

    // will compare different update path with 2 identical realms;
    context::Realm duplicate(main);

    A::Actions::fastJob(main, -5);
    A::Actions::slowJob(duplicate, -5);

    EXPECT_EQ(ask::item::get<A>(main, ids.at(0))->value, 0);
    EXPECT_EQ(ask::item::get<A>(main, ids.at(5))->value, 0);
    EXPECT_EQ(ask::item::get<A>(main, ids.at(6))->value, 1);

    EXPECT_EQ(ask::item::get<A>(main, ids.at(0))->value, ask::item::get<A>(duplicate, ids.at(0))->value);
    EXPECT_EQ(ask::item::get<A>(main, ids.at(5))->value, ask::item::get<A>(duplicate, ids.at(5))->value);
    EXPECT_EQ(ask::item::get<A>(main, ids.at(6))->value, ask::item::get<A>(duplicate, ids.at(6))->value);
}

} // namespace tests
