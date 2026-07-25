// Living HOW-TO (exemplar): relations, reactions, private Internals.
// See: test/features/HOWTO_entity_relations.md
#include "_common.h"

#include <fQSM/api/interface.h>

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
            static void tearOffTrunk(Writing context, Id victim) {
                const auto& elephant = my::get(context, victim);
                if (not elephant.myTrunk.has_value())
                    return;
                const auto trunkId = *elephant.myTrunk;
                my::modify(context, victim)->myTrunk = std::nullopt;
                if (with<Trunk>::exists(context, trunkId))
                    with<Trunk>::remove(context, trunkId);
            }

            // Envy: living trunk vs another's higher by > gap → tear the other's trunk off.
            static void envyTearOffs(Writing context) {
                for (const auto envious : context->aspect<Elephant>().items()) {
                    const auto* myTrunk = my::ward(context, envious.id, &Quantum::myTrunk);
                    if (not myTrunk)
                        continue;
                    const float myAngle = myTrunk->angleDegrees;
                    for (const auto other : context->aspect<Elephant>().items()) {
                        if (other.id == envious.id)
                            continue;
                        const auto* theirTrunk = my::ward(context, other.id, &Quantum::myTrunk);
                        if (not theirTrunk)
                            continue;
                        if (theirTrunk->angleDegrees > myAngle + envyAngleGapDegrees)
                            tearOffTrunk(context, other.id);
                    }
                }
            }

            static void onWorldTick(Reacting context) {
                bool worldMoved = false;
                for (const auto& change : context.changes<World>().updated()) {
                    if (change.after) {
                        worldMoved = true;
                        break;
                    }
                }
                if (not worldMoved)
                    return;

                Writing writing{context};
                for (const auto entry : context.proposal.aspect<Elephant>().items())
                    syncTrunkToMood(writing, entry.id);
                envyTearOffs(writing);
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

        static Id spawn(Writing context, World::Id world, integer mood) {
            const auto trunk = with<Trunk>::create(context, {.angleDegrees = 0.f});
            return with<Elephant>::create(context, {
                .world = world,
                .myTrunk = trunk,
                .mood = mood,
            });
        }

        static void afflict(Writing context, Elephant::Id elephant) {
            with<Disappointment>::create(context, {
                .target = elephant,
                .remains = standardAfflictionHours,
            });
        }
    };

    struct Disappointment::Internals : DefaultInternals {
        // dt hours from World clock: drain mood and charge; zero charge removes self.
        static void applyTimePassage(Writing context, Id id, integer dt) {
            if (dt <= 0)
                return;

            const auto* target = my::demand(context, id, &Quantum::target);
            if (not target)
                return;

            with<Elephant>::modify(context, my::get(context, id).target)->mood -= dt;

            my::modify(context, id)->remains -= dt;
            if (my::get(context, id).remains <= 0)
                my::remove(context, id);
        }

        static void onWorldClock(Reacting context) {
            Writing writing{context};
            Retrospecting past{context.retrospective};
            for (const auto& change : context.changes<World>().updated()) {
                if (not change.after)
                    continue;
                const auto pastTime = with<World>::get(past, change.id).time;
                const integer dt = static_cast<integer>(change.after->time - pastTime);
                for (const auto entry : context.proposal.aspect<Disappointment>().items())
                    applyTimePassage(writing, entry.id, dt);
            }
        }

        // Explicit detach Some → nullopt → afflict the victim (no friend groups).
        static void onElephantTrunkLoss(Reacting context) {
            Writing writing{context};
            for (const auto& change : context.changes<Elephant>().updated()) {
                if (not change.after)
                    continue;
                const auto& before = change.throwing_before();
                if (not (before.myTrunk.has_value() and not change.after->myTrunk.has_value()))
                    continue;
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
        ask::schema::aspect<Trunk>(),
        ask::schema::aspect<Elephant>(),
        ask::schema::aspect<Disappointment>(),
    });

    establish::Realm main(schema);

    const auto world = with<World>::create(main, {.time = 0});
    for (integer mood = 0; mood < 10; ++mood)
        with<Rules>::spawn(main, world, mood);

    for (int step = 0; step < 10; ++step)
        with<World>::modify(main, world)->time += 1;
}

} // namespace tests
