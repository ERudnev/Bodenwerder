// Living HOW-TO (exemplar): relations, reactions, private Internals.
// See: test/features/HOWTO_entity_relations.md
#include "_common.h"

#include <fQSM/api/interface.h>

#include <algorithm>
#include <format>
#include <vector>

namespace {
namespace local {
    using namespace fqsm::api;

    struct World : Entity<World> {
        struct Quantum {
            int time;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    // Chronicle of important decisions. Known to everyone below (not to World).
    struct History : Entity<History> {
        struct Quantum {
            World::Id world;
            int turn;
            string text;
        };
        struct Internals : DefaultInternals {
            static void note(Writing context, World::Id world, int turn, string text) {
                with<History>::create(context, {
                    .world = world,
                    .turn = turn,
                    .text = std::move(text),
                });
            }
        };
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Trunk : Entity<Trunk> {
        struct Quantum {
            float angleDegrees;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Elephant : Entity<Elephant> {
        struct Quantum {
            Anchor<World> world; // NB: current implementation of anchor links is not effective and thing twice before choosing this type
            optional<Custody<Trunk>> myTrunk; // nullopt == explicitly no trunk (detach is a field change)
            integer mood;
            string name;
        };
        struct Internals : DefaultInternals {
            static constexpr float envyAngleGapDegrees = 30.f;

            static void syncTrunkToMood(Writing context, Id id) {
                const auto* trunk = my::ward(context, id, &Quantum::myTrunk);
                if (not trunk)
                    return;
                const auto& me = my::get(context, id);
                const float angle = static_cast<float>(me.mood * 10);
                with<Trunk>::modify(context, *me.myTrunk)->angleDegrees =
                    angle > 120.f ? 120.f : angle;
            }

            // Explicit detach: clear optional + remove Trunk entity (patch-safe if many act at once).
            static void tearOffTrunk(Writing context, Id victim, int turn, const string* byName) {
                const auto& elephant = my::get(context, victim);
                if (not elephant.myTrunk.has_value())
                    return;
                const auto trunkId = *elephant.myTrunk;
                my::modify(context, victim)->myTrunk = std::nullopt;
                if (with<Trunk>::exists(context, trunkId))
                    with<Trunk>::remove(context, trunkId);
                if (byName)
                    History::Internals::note(context, elephant.world, turn,
                        std::format("{} tore trunk from {}", *byName, elephant.name));
                else
                    History::Internals::note(context, elephant.world, turn,
                        std::format("{} lost trunk", elephant.name));
            }

            // Envy: living trunk vs another's higher by > gap → tear the other's trunk off.
            static void envyTearOffs(Writing context, int turn) {
                for (const auto envious : context->aspect<Elephant>().items()) {
                    const auto* myTrunk = my::ward(context, envious.id, &Quantum::myTrunk);
                    if (not myTrunk)
                        continue;
                    const float myAngle = myTrunk->angleDegrees;
                    const auto& enviousQ = my::get(context, envious.id);
                    for (const auto other : context->aspect<Elephant>().items()) {
                        if (other.id == envious.id)
                            continue;
                        const auto* theirTrunk = my::ward(context, other.id, &Quantum::myTrunk);
                        if (not theirTrunk)
                            continue;
                        if (theirTrunk->angleDegrees > myAngle + envyAngleGapDegrees)
                            tearOffTrunk(context, other.id, turn, &enviousQ.name);
                    }
                }
            }

            // Happiest alive gets +1 (ties: first max wins).
            static void boostHappiest(Writing context, int turn) {
                optional<Id> best;
                integer bestMood{};
                for (const auto e : context->aspect<Elephant>().items()) {
                    if (not best or e.value.mood > bestMood) {
                        best = e.id;
                        bestMood = e.value.mood;
                    }
                }
                if (not best)
                    return;
                my::modify(context, *best)->mood += 1;
                const auto& me = my::get(context, *best);
                History::Internals::note(context, me.world, turn,
                    std::format("{} mood +1 (happiest)", me.name));
            }

            // No trunk → −1 mood; mood < 0 → die of melancholy.
            static void trunklessSadnessAndMelancholy(Writing context, int turn) {
                for (const auto e : context->aspect<Elephant>().items()) {
                    if (not my::get(context, e.id).myTrunk.has_value())
                        my::modify(context, e.id)->mood -= 1;
                }
                std::vector<Id> doomed;
                for (const auto e : context->aspect<Elephant>().items()) {
                    if (my::get(context, e.id).mood < 0)
                        doomed.push_back(e.id);
                }
                for (const auto id : doomed) {
                    const auto& me = my::get(context, id);
                    History::Internals::note(context, me.world, turn,
                        std::format("{} died of melancholy", me.name));
                    my::remove(context, id);
                }
            }

            static void onWorldTick(Reacting context) {
                optional<int> turn;
                for (const auto& change : context.changes<World>().updated()) {
                    turn = change.now.time;
                    break;
                }
                if (not turn)
                    return;

                Writing writing{context};
                for (const auto entry : context.proposal.aspect<Elephant>().items())
                    syncTrunkToMood(writing, entry.id);
                envyTearOffs(writing, *turn);
                boostHappiest(writing, *turn);
                trunklessSadnessAndMelancholy(writing, *turn);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<Elephant, World>(&Internals::onWorldTick),
            };
        }
    };

    struct Disappointment : Entity<Disappointment> {
        struct Quantum {
            Affected<Elephant> target;
            integer remains;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Rules : Manipulation<Rules, Elephant> {
        // Narrative (-3): three hours of disappointment charge.
        static constexpr integer standardAfflictionHours = 3;

        static Id spawn(Writing context, World::Id world, integer mood, string name) {
            const auto trunk = with<Trunk>::create(context, {.angleDegrees = 0.f});
            return with<Elephant>::create(context, {
                .world = world,
                .myTrunk = trunk,
                .mood = mood,
                .name = std::move(name),
            });
        }

        static void afflict(Writing context, Elephant::Id elephant) {
            const auto& e = with<Elephant>::get(context, elephant);
            const int turn = with<World>::get(context, e.world).time;
            with<Disappointment>::create(context, {
                .target = elephant,
                .remains = standardAfflictionHours,
            });
            History::Internals::note(context, e.world, turn,
                std::format("{} afflicted (−{})", e.name, standardAfflictionHours));
        }
    };

    struct Disappointment::Internals : DefaultInternals {
        // dt hours from World clock: drain mood and charge; zero charge removes self.
        static void applyTimePassage(Writing context, Id id, integer dt, int turn) {
            const auto* target = my::vital(context, id, &Quantum::target);
            if (not target)
                return;

            with<Elephant>::modify(context, my::get(context, id).target)->mood -= dt;

            my::modify(context, id)->remains -= dt;
            if (my::get(context, id).remains <= 0) {
                const auto& e = with<Elephant>::get(context, my::get(context, id).target);
                History::Internals::note(context, e.world, turn,
                    std::format("{} disappointment ended", e.name));
                my::remove(context, id);
            }
        }

        static void onWorldClock(Reacting context) {
            Writing writing{context};
            for (const auto& change : context.changes<World>().updated()) {
                const integer dt = static_cast<integer>(change.now.time - change.old.time);
                const int turn = change.now.time;
                for (const auto entry : context.proposal.aspect<Disappointment>().items())
                    applyTimePassage(writing, entry.id, dt, turn);
            }
        }

        // Explicit detach Some → nullopt → afflict the victim (no friend groups).
        static void onElephantTrunkLoss(Reacting context) {
            Writing writing{context};
            for (const auto& change : context.changes<Elephant>().updated()) {
                if (field_event(change, &Elephant::Quantum::myTrunk).removed)
                    with<Rules>::afflict(writing, change.id);
            }
        }
    };

    auto Disappointment::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Disappointment, World>(&Internals::onWorldClock),
            reaction::aspect_wide<Disappointment, Elephant>(&Internals::onElephantTrunkLoss),
        };
    };
}
} // namespace

namespace tests {

void entity_relations()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<World>(),
        ask::schema::aspect<History>(),
        ask::schema::aspect<Trunk>(),
        ask::schema::aspect<Elephant>(),
        ask::schema::aspect<Disappointment>(),
    });

    establish::Realm main(schema);

    const auto world = with<World>::create(main, {.time = 0});
    for (integer mood = 0; mood < 10; ++mood)
        with<Rules>::spawn(main, world, mood, std::format("elephant{}", mood + 1));

    for (int step = 0; step < 10; ++step)
        with<World>::modify(main, world)->time += 1;

    EXPECT_EQ(tests::debug::count<Elephant>(fqsm::Reading(main)), 5)
        << "envy + melancholy leave five (1–4 with trunks, 10 trunkless champion)";

    /* uncomment just as fun. Or to feed your LLM with Trunk Story
    std::vector<item<History>> chronicle;
    for (const auto entry : fqsm::Reading(main)->aspect<History>().items())
        chronicle.push_back(entry.value);
    std::stable_sort(chronicle.begin(), chronicle.end(),
        [](const auto& a, const auto& b) { return a.turn < b.turn; });

    base::message("── History ──");
    for (const auto& event : chronicle)
        base::message("[turn {}] {}", event.turn, event.text);
    */
}

} // namespace tests
