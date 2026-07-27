#include "_common.h"

#include <format>
#include <string>
#include <vector>

#include <fQSM/api/interface.h>

namespace {

namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    // Good: inbound index on A.removed → delete hangers.
    struct D : Entity<D> {
        struct Quantum {
            Affected<A> target;
        };
        struct Internals : DefaultInternals {
            static void on_a_removed(Reacting context) {
                const auto holders = ask::relations<A>(context).removed<D, &D::Quantum::target>();
                for (const auto& change : context.changes<A>().removed()) {
                    for (const auto id : holders.ids(change.id))
                        with<D>::remove(context, id);
                }
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<D, A>(&Internals::on_a_removed),
            };
        }
    };

    // Bad: nested scan of all E for each removed A.
    struct E : Entity<E> {
        struct Quantum {
            Affected<A> target;
        };
        struct Internals : DefaultInternals {
            static void on_a_removed(Reacting context) {
                for (const auto& change : context.changes<A>().removed()) {
                    for (const auto entry : context.proposal.aspect<E>().items()) {
                        if (entry.value.target != change.id)
                            continue;
                        with<E>::remove(context, entry.id);
                    }
                }
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<E, A>(&Internals::on_a_removed),
            };
        }
    };
}

} // namespace

namespace tests {

using namespace local;
using namespace fqsm::api;

constexpr int a_count = 10;
// Enough holders that remove of the packed A is a visible chunk of work.
constexpr integer holders_per_side = 40000;

void relations_index_performance()
{
    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<D>(),
        ask::schema::aspect<E>(),
    });

    establish::Realm main(schema);

    std::vector<A::Id> hubs;
    hubs.reserve(static_cast<std::size_t>(a_count));

    const auto setup_label = std::format(
        "relations_index_performance: create {} A + {} D + {} E",
        a_count,
        holders_per_side,
        holders_per_side);
    {
        testing::scoped_timer timer(setup_label.c_str());
        main.branch([&](Writing context) {
            for (int i = 0; i < a_count; ++i)
                hubs.push_back(with<A>::create(context, {}));

            const auto a_for_d = hubs.at(0);
            const auto a_for_e = hubs.at(1);

            for (integer i = 0; i < holders_per_side; ++i) {
                with<D>::create(context, {.target = a_for_d});
                with<E>::create(context, {.target = a_for_e});
            }
        });
    }
    EXPECT_TRUE(main.result().good());
    EXPECT_EQ(with<A>::count(main), static_cast<std::size_t>(a_count));
    EXPECT_EQ(with<D>::count(main), static_cast<std::size_t>(holders_per_side));
    EXPECT_EQ(with<E>::count(main), static_cast<std::size_t>(holders_per_side));

    const auto a_for_d = hubs.at(0);
    const auto a_for_e = hubs.at(1);

    // Both D and E listen to A, so each remove arms both reactions.
    // Remove E's hub first (naive path does the mass delete); D's indexed miss still
    // scans D. Then remove D's hub (indexed mass delete; E table already empty).
    {
        testing::scoped_timer timer(
            "relations_index_performance: remove A of all E (without index)");
        with<A>::remove(main, a_for_e);
    }
    EXPECT_TRUE(main.result().good());
    EXPECT_EQ(with<E>::count(main), std::size_t{0});
    EXPECT_EQ(with<D>::count(main), static_cast<std::size_t>(holders_per_side));

    {
        testing::scoped_timer timer(
            "relations_index_performance: remove A of all D (with index)");
        with<A>::remove(main, a_for_d);
    }
    EXPECT_TRUE(main.result().good());
    EXPECT_EQ(with<D>::count(main), std::size_t{0});
    EXPECT_EQ(with<A>::count(main), static_cast<std::size_t>(a_count - 2));
}

} // namespace tests
