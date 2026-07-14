#include "_common.h"

#include <fQSM/api/interface.h>

// kinda header:
namespace {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            integer value;
        };
        struct Actions : BaseActions {
            static void fastJob(Direct<A> context, int bonus) {
                for (auto [_, item] : context.items)
                    item.value += bonus;
            }
            static void slowJob(Writing context, int bonus) {
                auto& items = context->aspect<A>().items();
                for (auto [id, item] : items)
                    with<A>::modify(context, id)->value += bonus;
            }
            /* TODO: this is other story, "modern" access to patch through Draft interface (needs reworked Gate)
            static void modernJob(ModernGate context, int bonus) {
                for (auto [_, item] : context->aspect<A>().items())
                    item.value += bonus;
            }*/
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };
}

// kinda CPP part:
namespace {
    using namespace fqsm::api;

    struct A::Internals : DefaultInternals {
        static auto allow_non_negative(const Quantum& data) -> PossibleChange {
            if (data.value >= 0)
                return std::nullopt;
            return Quantum{.value = 0};
        }
    };

    auto A::customAspectReactions() -> const Behavior {
        return {
            reaction::constraint::element<A>(&Internals::allow_non_negative),
        };
    }
}

// test itself:
namespace tests {

void immediate()
{
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
    });

    establish::Realm main(schema);

    std::vector<A::Id> ids;
    main.branch([&](Writing context) {
        for (int xx = 0; xx < 10; ++xx)
            ids.push_back(with<A>::create(context, {xx}));
    });

    // will compare different update path with 2 identical realms;
    establish::Realm duplicate(main);

    with<A>::fastJob(main, -5);
    with<A>::slowJob(duplicate, -5);

    EXPECT_EQ(with<A>::get(main, ids.at(0)).value, 0);
    EXPECT_EQ(with<A>::get(main, ids.at(5)).value, 0);
    EXPECT_EQ(with<A>::get(main, ids.at(6)).value, 1);

    EXPECT_EQ(with<A>::get(main, ids.at(0)).value, with<A>::get(duplicate, ids.at(0)).value);
    EXPECT_EQ(with<A>::get(main, ids.at(5)).value, with<A>::get(duplicate, ids.at(5)).value);
    EXPECT_EQ(with<A>::get(main, ids.at(6)).value, with<A>::get(duplicate, ids.at(6)).value);
}

} // namespace tests
