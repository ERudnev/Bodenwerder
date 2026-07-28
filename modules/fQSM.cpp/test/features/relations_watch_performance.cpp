#include "_common.h"

#include <format>
#include <optional>
#include <string>
#include <vector>

#include <fQSM/api/interface.h>

namespace {

namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            integer noise = 0;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    // Watchers of "their" A via bare optional<Id> — indexed path.
    struct Bix : Entity<Bix> {
        struct Quantum {
            std::optional<A::Id> focus;
            integer ticks = 0;
        };
        struct Internals : DefaultInternals {
            static void on_a_updated(Reacting context) {
                const auto by_focus = ask::relations<A>(context).updated<Bix, &Bix::Quantum::focus>();
                for (const auto& change : context.changes<A>().updated()) {
                    for (auto [id, item] : by_focus.items(change.id)) {
                        with<Bix>::modify(context, id)->ticks = item.ticks + 1;
                    }
                }
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<Bix, A>(&Internals::on_a_updated),
            };
        }
    };

    // Same link shape — nested scan (no index).
    struct Bnv : Entity<Bnv> {
        struct Quantum {
            std::optional<A::Id> focus;
            integer ticks = 0;
        };
        struct Internals : DefaultInternals {
            static void on_a_updated(Reacting context) {
                for (const auto& change : context.changes<A>().updated()) {
                    for (const auto entry : context.proposal.aspect<Bnv>().items()) {
                        if (entry.value.focus != change.id)
                            continue;
                        with<Bnv>::modify(context, entry.id)->ticks = entry.value.ticks + 1;
                    }
                }
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<Bnv, A>(&Internals::on_a_updated),
            };
        }
    };

    template<fqsm::category::Any Watcher>
    auto run_watch_side(
        const char* setup_label,
        const char* pulse_label,
        int a_count,
        integer watchers_per_a,
        int rounds) -> void
    {
        const Schema schema = ask::schema::merge({
            ask::schema::aspect<A>(),
            ask::schema::aspect<Watcher>(),
        });

        establish::Realm main(schema);

        std::vector<A::Id> hubs;
        hubs.reserve(static_cast<std::size_t>(a_count));
        std::optional<typename Watcher::Id> sample_watcher;

        {
            testing::scoped_timer timer(setup_label);
            main.branch([&](Writing context) {
                for (int i = 0; i < a_count; ++i)
                    hubs.push_back(with<A>::create(context, {}));

                for (int ai = 0; ai < a_count; ++ai) {
                    const auto hub = hubs.at(static_cast<std::size_t>(ai));
                    for (integer wi = 0; wi < watchers_per_a; ++wi) {
                        const auto id = with<Watcher>::create(context, {.focus = hub});
                        if (not sample_watcher.has_value())
                            sample_watcher = id;
                    }
                }
            });
        }
        EXPECT_TRUE(main.result().good());
        EXPECT_EQ(with<A>::count(main), static_cast<std::size_t>(a_count));
        EXPECT_EQ(
            with<Watcher>::count(main),
            static_cast<std::size_t>(a_count) * static_cast<std::size_t>(watchers_per_a));
        EXPECT_TRUE(sample_watcher.has_value());

        {
            testing::scoped_timer timer(pulse_label);
            for (int round = 0; round < rounds; ++round) {
                main.branch([&](Writing context) {
                    for (const auto hub : hubs)
                        with<A>::modify(context, hub)->noise = round;
                });
            }
        }
        EXPECT_TRUE(main.result().good());
        EXPECT_EQ(with<Watcher>::get(main, *sample_watcher).ticks, integer{rounds});
    }
}

} // namespace

namespace tests {

using namespace local;
using namespace fqsm::api;

// Many A, many optional-Id watchers; each round updates all A in one writing.
// Indexed: O(|B|) per round. Naive: O(|A|·|B|) per round.
constexpr int a_count = 100;
constexpr integer watchers_per_a = 100;
constexpr int rounds = 4;

void relations_watch_performance()
{
    const auto total_b = a_count * watchers_per_a;

    const auto setup_ix = std::format(
        "relations_watch_performance: create {} A + {} Bix (optional Id, indexed)",
        a_count,
        total_b);
    const auto pulse_ix = std::format(
        "relations_watch_performance: {} rounds x update all A (with index)",
        rounds);
    run_watch_side<Bix>(setup_ix.c_str(), pulse_ix.c_str(), a_count, watchers_per_a, rounds);

    const auto setup_nv = std::format(
        "relations_watch_performance: create {} A + {} Bnv (optional Id, naive)",
        a_count,
        total_b);
    const auto pulse_nv = std::format(
        "relations_watch_performance: {} rounds x update all A (without index)",
        rounds);
    run_watch_side<Bnv>(setup_nv.c_str(), pulse_nv.c_str(), a_count, watchers_per_a, rounds);
}

} // namespace tests
