#include "_common.h"

#include <fQSM/api/interface.h>

// Feature co-birth vs Stewarding Direct taint of the host aspect.
// Eltanin: spawn Body+Solid / Thing+Scrap from a Dock that already opened direct<Host>().
namespace {
    using namespace fqsm::api;

    struct Host : Entity<Host> {
        struct Quantum {
            integer value;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Cap : Feature<Cap, Host> {
        struct Quantum {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Other : Entity<Other> {
        struct Quantum {
            integer value;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    auto spawnPair(Writing context, integer value) -> Host::Id {
        const auto id = with<Host>::create(context, {.value = value});
        with<Cap>::extend(context, id, {});
        return id;
    }

    auto criticalText(const auto& result) -> string {
        string text;
        for (const auto& msg : result.critical) {
            if (not text.empty())
                text += "; ";
            text += msg;
        }
        return text;
    }
}

namespace tests {

void direct_taint_feature_birth()
{
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Host>(),
        ask::schema::aspect<Cap>(),
        ask::schema::aspect<Other>(),
    });

    {
        establish::Realm main(schema);
        const auto id = spawnPair(main, 1);
        EXPECT_TRUE(main.result().good()) << criticalText(main.result());
        EXPECT_TRUE(with<Host>::exists(main, id));
        EXPECT_TRUE(with<Cap>::exists(main, id));
    }

    {
        establish::Realm main(schema);
        const auto id = [&] {
            Stewarding session = main;
            return spawnPair(session, 1);
        }();
        EXPECT_TRUE(main.result().good()) << criticalText(main.result());
        EXPECT_TRUE(with<Host>::exists(main, id));
        EXPECT_TRUE(with<Cap>::exists(main, id));
    }

    {
        establish::Realm main(schema);
        const auto other = with<Other>::create(main, {.value = 0});
        const auto id = [&] {
            Stewarding session = main;
            auto others = session.direct<Other>();
            if (auto* item = others.items.find(other))
                item->value += 1;
            return spawnPair(session, 1);
        }();
        EXPECT_TRUE(main.result().good()) << criticalText(main.result());
        EXPECT_TRUE(with<Host>::exists(main, id));
        EXPECT_TRUE(with<Cap>::exists(main, id));
        EXPECT_EQ(with<Other>::get(main, other).value, 1);
    }

    {
        establish::Realm main(schema);
        const auto seed = with<Host>::create(main, {.value = 0});
        const auto id = [&] {
            Stewarding session = main;
            auto hosts = session.direct<Host>();
            if (auto* item = hosts.items.find(seed))
                item->value += 1;
            return spawnPair(session, 1);
        }();
        EXPECT_TRUE(main.result().good()) << criticalText(main.result());
        EXPECT_TRUE(with<Host>::exists(main, id));
        EXPECT_TRUE(with<Cap>::exists(main, id));
        EXPECT_EQ(with<Host>::get(main, seed).value, 1);
    }
}

}
