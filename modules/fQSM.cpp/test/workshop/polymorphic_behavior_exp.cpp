#include "_common.h"

#include <fQSM/api/interface.h>

namespace workshop {
    using namespace fqsm::api;

    struct Wallet : Entity<Wallet> {
        // Workshop exploration: spin the idea without pretending this is production-ready.
        // The function pointer is a concept, not the target model. Properly, Quantum
        // should hold persistent Ids: handler type and handler instance in the world. Turning
        // this into a real feature will be sweaty work.
        using IncomeStrategy = void(*)(fqsm::Writing, Id id, int income);

        struct Quantum {
            int cash;
            int stocks;
            IncomeStrategy incomeAction;
        };

        struct Actions : BaseActions {
            static void income(Writing context, Id wallet, int amount) {
                get(context, wallet).incomeAction(context, wallet, amount);
            }
        };

        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Steady : Attribute<Steady, Wallet> {
        struct Quantum {
            int lastIncomeMemory = 0;
        };

        struct Actions : BaseActions {
            static Wallet::Id spawn(Writing context, int cash) {
                const auto id = with<Wallet>::create(context, Wallet::Quantum{
                    .cash = cash,
                    .stocks = 0,
                    .incomeAction = &income,
                });
                extend(context, id, Quantum{.lastIncomeMemory = 0});
                return id;
            }

        private:
            static void income(Writing context, Wallet::Id wallet, int amount) {
                auto& mem = with<Steady>::modify(context, wallet)->lastIncomeMemory;
                auto w = with<Wallet>::modify(context, wallet);
                if (amount > 0) {
                    w->stocks += amount;
                } else {
                    w->cash += amount;
                }
                mem = amount;
            }
        };

        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Risky : Attribute<Risky, Wallet> {
        struct Quantum {};

        struct Actions : BaseActions {
            static Wallet::Id spawn(Writing context, int cash) {
                const auto id = with<Wallet>::create(context, Wallet::Quantum{
                    .cash = cash,
                    .stocks = 0,
                    .incomeAction = &income,
                });
                extend(context, id, Quantum{});
                return id;
            }

        private:
            static void income(Writing context, Wallet::Id wallet, int amount) {
                if (amount > 0) {
                    with<Wallet>::modify(context, wallet)->stocks += amount;
                }
            }
        };

        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };
}

namespace tests {

void polymorphic_behavior_exp()
{

    using namespace workshop;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Wallet>(),
        ask::schema::aspect<Steady>(),
        ask::schema::aspect<Risky>(),
    });

    establish::Realm main(schema);

    const auto steadyId = with<Steady>::spawn(main, 10);
    const auto riskyId = with<Risky>::spawn(main, 50);

    {
        establish::Branch tx(main);
        with<Wallet>::income(tx, steadyId, 100);
        with<Wallet>::income(tx, riskyId, 100);
    }

    // positive income: both strategies park it in stocks; starting cash untouched
    EXPECT_EQ(with<Wallet>::get(main, steadyId).cash, 10);
    EXPECT_EQ(with<Wallet>::get(main, steadyId).stocks, 100);
    EXPECT_EQ(with<Wallet>::get(main, riskyId).cash, 50);
    EXPECT_EQ(with<Wallet>::get(main, riskyId).stocks, 100);

}

} // namespace tests
