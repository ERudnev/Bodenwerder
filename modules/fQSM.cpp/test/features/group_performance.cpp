#include "_common.h"

#include <fQSM/api/interface.h>

namespace {

namespace local {
    using namespace fqsm::api;

    struct Stump : Entity<Stump> {
        struct Quantum {
            integer index = 0;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Speck : Entity<Speck> {
        struct Quantum {
            integer tag = 0;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Speck_group : Group<Speck_group, Stump, Speck> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Grove : Archetype<Grove> {
        static Stump::Id
        plant(Writing context, Stump::Quantum stumpValue) {
            const auto stump = with<Stump>::create(context, std::move(stumpValue));
            with<Speck_group>::extend(context, stump);
            return stump;
        }

        static Speck::Id
        addSpeck(Writing context, Stump::Id stump, Speck::Quantum speckValue) {
            return with<Speck_group>::addElement(context, stump, std::move(speckValue));
        }
    };
}

} // namespace

namespace tests {

using namespace local;
using namespace fqsm::api;

constexpr integer elementCount = 10000;

void group_performance_bulk_create_delete(fqsm::api::Schema schema)
{
    establish::Realm main(schema);

    const auto stump = with<Grove>::plant(main, {});

    main.branch([&](Writing context) {
        testing::scoped_timer timer("create {} elements in one group");
        for (integer index = 0; index < elementCount; ++index)
            with<Grove>::addSpeck(context, stump, {.tag = index});
    });

    main.branch([&](Writing context) {
        testing::scoped_timer timer("delete {} elements from one group");
        const auto group = with<Speck_group>::get(context, stump);
        for (const auto speck : group)
            with<Speck_group>::deleteElement(context, stump, speck);
        with<Stump>::remove(context, stump);
    });
}

void group_performance()
{
    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Stump>(),
        ask::schema::aspect<Speck>(),
        ask::schema::aspect<Speck_group>(),
    });

    group_performance_bulk_create_delete(schema);
}

} // namespace tests
