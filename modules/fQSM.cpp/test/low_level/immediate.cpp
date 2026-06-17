#include "_common.h"

#include <fQSM/api/interface.h>
#include <fQSM/model/complex/data.h>

// kinda header:
namespace {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            integer value;
        };
        static const Codex codex;
        struct Actions : BaseActions {
            struct Private;
            static void fastJob(Immediate<A> context, int bonus) {
                for (auto& item : context.items)
                    item->value += bonus;
                //if (context.items.size() > 3)
                //    context.items[3] = context.items[2];
            }
        };
    };
}

// kinda CPP part:
namespace {
    using namespace fqsm::api;

    struct A::Actions::Private : A::Actions {
        static auto normalize_value(const Quantum& data) -> Update {
            if (data.value >= 0) return std::nullopt;
            return Quantum{.value = 0};
        }
    };

    const A::Codex A::codex = {
        norma::local<A>(&A::Actions::Private::normalize_value),
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

    fqsm::state::world::Data world(schema);
    context::Realm main(world);

    std::vector<A::Id> ids;
    {
        context::Branch local(main);
        for (int xx = 0; xx < 10; ++xx)
            ids.push_back(ask::item::create<A>(local, {xx}));
    }

    A::Actions::fastJob(main, -5);

    EXPECT_EQ(ask::item::get<A>(main, ids.at(0))->value, 0);
    EXPECT_EQ(ask::item::get<A>(main, ids.at(5))->value, 0);
    EXPECT_EQ(ask::item::get<A>(main, ids.at(6))->value, 1);
}

} // namespace tests
