#include "_common.h"

#include <fQSM/api/interface.h>

// The inbound index behind custody: holders of a ward are found through the Reality's index, corrected by the
// transaction's own layers. Cases: a holder created in the same transaction that removes the ward; a holder
// re-linked away in that transaction; a link changed in place through direct<> (the index is stale until the
// transaction ends, the scan answers, then the index is rebuilt); a Realm copied from another rebuilds its index.
namespace {
    using namespace fqsm::api;

    namespace local {
        struct Ward : Entity<Ward> {
            struct Quantum {
                integer weight = 0;
            };
        };
        struct Holder : Entity<Holder> {
            struct Quantum {
                Custody<Ward> ward;
                integer tag = 0;
            };
            static const Behavior customAspectReactions();
        };
        auto Holder::customAspectReactions() -> const Behavior {
            return {
                reaction::structural::custody<Holder, Ward, &Holder::Quantum::ward>{},
            };
        }
        Schema schema() {
            return ask::schema::merge({ask::schema::aspect<Ward>(), ask::schema::aspect<Holder>()});
        }
    }
}

namespace tests {

void inbound_index()
{
    using namespace local;

    { // the schema knows the link once, even when the reaction is registered twice
        const Schema once = schema();
        EXPECT_EQ(once->links.size(), std::size_t{1});
        const Schema twice = ask::schema::merge({schema(), schema()});
        EXPECT_EQ(twice->links.size(), std::size_t{1});
    }

    { // holder created in the transaction that removes the ward
        establish::Realm main(schema());
        const auto a = with<Ward>::create(main, {});
        const auto h1 = with<Holder>::create(main, {.ward = a});
        Holder::Id h2 = main.branch([&](Writing context) {
            const auto made = with<Holder>::create(context, {.ward = a, .tag = 2});
            with<Ward>::remove(context, a);
            return made;
        });
        EXPECT_TRUE(main.result().good());
        EXPECT_FALSE(with<Holder>::exists(main, h1)) << "indexed holder dies with the ward";
        EXPECT_FALSE(with<Holder>::exists(main, h2)) << "holder from the pending layer dies with the ward";
    }

    { // holder re-linked away in the transaction that removes the old ward
        establish::Realm main(schema());
        const auto a = with<Ward>::create(main, {});
        const auto b = with<Ward>::create(main, {});
        const auto h = with<Holder>::create(main, {.ward = a});
        {
            Writing context = main;
            with<Holder>::modify(context, h)->ward = b;
            with<Ward>::remove(context, a);
        }
        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(with<Holder>::exists(main, h)) << "the pending value points at b, not at the removed a";
        EXPECT_TRUE(with<Ward>::exists(main, b));
        with<Ward>::remove(main, b);
        EXPECT_FALSE(with<Holder>::exists(main, h)) << "the index learned the new link at integration";
    }

    { // link changed in place: the index is stale inside the transaction, rebuilt at its end
        establish::Realm main(schema());
        const auto a = with<Ward>::create(main, {});
        const auto b = with<Ward>::create(main, {});
        const auto h = with<Holder>::create(main, {.ward = a});
        {
            Stewarding session = main;
            session.direct<Holder>().items.find(h)->ward = b;
            with<Ward>::remove(session, b);
        }
        EXPECT_TRUE(main.result().good());
        EXPECT_FALSE(with<Holder>::exists(main, h)) << "the scan sees the in-place link to b";
        const auto h2 = with<Holder>::create(main, {.ward = a});
        {
            Stewarding session = main;
            session.direct<Holder>().items.find(h2)->tag = 5;   // taint without a link change
        }
        with<Ward>::remove(main, a);
        EXPECT_FALSE(with<Holder>::exists(main, h2)) << "the index was rebuilt after the direct pass";
    }

    { // a Realm built from another state rebuilds the index
        establish::Realm main(schema());
        const auto a = with<Ward>::create(main, {});
        const auto h = with<Holder>::create(main, {.ward = a});
        establish::Realm copy(main);
        EXPECT_TRUE(with<Holder>::exists(copy, h));
        with<Ward>::remove(copy, a);
        EXPECT_TRUE(copy.result().good());
        EXPECT_FALSE(with<Holder>::exists(copy, h)) << "the copy's index knows the holder";
        EXPECT_TRUE(with<Holder>::exists(main, h)) << "the source is untouched";
    }
}

} // namespace tests
