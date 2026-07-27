// TDD pad: inbound index A ← B.target (Affected) via ask::relations<A>(…).to<B, &…>().
// Half-measure: rebuild map each reaction call. No scenario / expects yet.
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
            const auto indexation = ask::relations<A>(context).to<B, &B::Quantum::target>();
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
    });

    fqsm::model::complex::Reality world(schema);
    establish::Realm main(world);

    // TDD playground: add fuel + expects when ask::relations lands.
}

}
