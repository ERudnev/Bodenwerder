// TDD pad: inbound index via ask::relations (Affected) + monster C (Anchor/Custody).
// Half-measure: rebuild map each reaction call, scoped to delta layer (removed/updated/…).
// No scenario / expects yet.
#include "_common.h"

#include <fQSM/api/interface.h>

#include <vector>

namespace {
namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            integer data_field;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct B : Entity<B> {
        struct Quantum {
            Affected<A> target;
            integer trigger_value;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    // Monster: Anchor→A + Custody→B; two hand reactions + codegen structural (anchor + custody).
    struct C : Entity<C> {
        struct Quantum {
            Anchor<A> hub;
            Custody<B> kept;
            integer ticks = 0;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct B::Internals : B::DefaultInternals {
        static auto reached_by(
            const Quantum& watcher,
            A::Id a_id,
            const A::Quantum& a_now) -> bool
        {
            return watcher.target == a_id
                and a_now.data_field == watcher.trigger_value;
        }

        // !on_a_reached(~A) — object reaction; inbound index then probe by changed A
        static void on_a_reached(Reacting context) {
            std::vector<Id> reached;
            const auto indexation = ask::relations<A>(context).updated<B, &B::Quantum::target>();
            for (const auto& change : context.changes<A>().updated()) {
                for (auto [id, item] : indexation.items(change.id)) {
                    if (not reached_by(item, change.id, change.now))
                        continue;
                    reached.push_back(id);
                }
            }
            for (const auto id : reached)
                with<B>::remove(context, id);
        }
    };

    auto B::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<B, A>(&Internals::on_a_reached),
        };
    }

    struct C::Internals : C::DefaultInternals {
        // Degenerate: each A update that matches hub → ticks++
        static void pulse_on_hub(Reacting context) {
            const auto by_hub = ask::relations<A>(context).updated<C, &C::Quantum::hub>();
            for (const auto& change : context.changes<A>().updated()) {
                for (auto [id, item] : by_hub.items(change.id)) {
                    with<C>::modify(context, id)->ticks = item.ticks + 1;
                }
            }
        }
    };

    auto C::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<C, A>(&Internals::pulse_on_hub),
            reaction::structural::anchored<C, A, &C::Quantum::hub>{},
            reaction::structural::custody<C, B, &C::Quantum::kept>{},
        };
    }
}
} // namespace

namespace tests {

void relations_index_build()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
        ask::schema::aspect<C>(),
    });

    fqsm::model::complex::Reality world(schema);
    establish::Realm main(world);

    // TDD playground: fuel + expects later (Affected index; later Anchor/Custody index).
}

}
