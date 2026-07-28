#include <tommy/invaders/combat.h>

#include <tommy/invaders/actors.h>
#include <tommy/invaders/session.h>
#include <tommy/invaders/visual.h>
#include <tommy/world.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        constexpr index2 k_shot_half{3, 10};
        constexpr index2 k_player_half{20, 14};
        constexpr index2 k_alien_half{18, 15};

    } // namespace

    auto Shot::sprite_index(Side side) -> integer {
        switch (side) {
            case Side::player: return sprite_player;
            case Side::alien: return sprite_alien;
        }
        return sprite_player;
    }

    struct Shot::Internals : Shot::DefaultInternals {
        static void destroyShot(Writing context, Shot::Id shot_id) {
            if (not with<Shot>::exists(context, shot_id)) {
                return;
            }
            const auto session = with<Shot>::get(context, shot_id).session;
            destroyGameObjectSprite(context, shot_id);
            if (with<Shot_group>::exists(context, session)) {
                with<Shot_group>::deleteElement(context, session, shot_id);
            } else {
                with<GameObject>::remove(context, shot_id);
            }
        }

        static void resolveHit(Writing context, Shot::Id shot_id, Session::Id session_id) {
            const auto& shot = with<Shot>::get(context, shot_id);

            const auto player = with<Session>::get(context, session_id).player;
            if (shot.side == Side::alien and player and with<Player>::exists(context, *player)) {
                const auto& player_q = with<Player>::get(context, *player);
                if (aabbOverlap(shot.pos, k_shot_half, player_q.pos, k_player_half)) {
                    notePlayerHit(context, session_id);
                    destroyShot(context, shot_id);
                }
                return;
            }

            if (shot.side != Side::player
                or not with<Fleet>::exists(context, session_id)
                or not with<Alien_group>::exists(context, session_id))
            {
                return;
            }

            const auto& fleet = with<Fleet>::get(context, session_id);
            for (const auto alien_id : with<Alien_group>::get(context, session_id)) {
                auto alien = with<Alien>::modify(context, alien_id);
                if (not alien->alive) {
                    continue;
                }
                const index2 pos = alienWorldPos(fleet, alien->cell);
                if (not aabbOverlap(shot.pos, k_shot_half, pos, k_alien_half)) {
                    continue;
                }
                alien->alive = false;
                destroyGameObjectSprite(context, alien_id);
                with<Session>::modify(context, session_id)->score += alien->points;

                bool any_alive = false;
                for (const auto id : with<Alien_group>::get(context, session_id)) {
                    if (with<Alien>::get(context, id).alive) {
                        any_alive = true;
                        break;
                    }
                }
                if (not any_alive) {
                    noteFleetCleared(context, session_id);
                }
                destroyShot(context, shot_id);
                return;
            }
        }

        static void advanceMotion(Writing context, Shot::Id shot_id, integer steps) {
            const auto session_id = with<Shot>::get(context, shot_id).session;
            if (not with<Session>::exists(context, session_id)) {
                destroyShot(context, shot_id);
                return;
            }

            auto shot = with<Shot>::modify(context, shot_id);
            if (shot->side == Side::player) {
                shot->pos.y += player_speed * steps;
            } else {
                shot->motion_carry += alien_speed * steps;
                shot->pos.y -= shot->motion_carry / alien_speed_period;
                shot->motion_carry %= alien_speed_period;
            }
            syncGameObjectSprite(context, shot_id, shot->pos);

            const auto& field = with<Playfield>::get(context, session_id);
            if (shot->pos.y > field.origin.y + field.size.y + 40
                or shot->pos.y < field.origin.y - 40)
            {
                destroyShot(context, shot_id);
                return;
            }

            resolveHit(context, shot_id, session_id);
        }

        static void advance(Reacting context) {
            const auto by_world = ask::relations<World>(context).updated<Session, &Session::Quantum::world>();
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step or change.now.paused) {
                    continue;
                }
                const integer steps = change.now.step - change.old.step;

                for (const auto session_id : by_world.ids(change.id)) {
                    if (not sessionPlaying(context, session_id)) {
                        continue;
                    }
                    if (not with<Shot_group>::exists(context, session_id)) {
                        continue;
                    }
                    for (const auto shot_id : with<Shot_group>::get(context, session_id)) {
                        advanceMotion(context, shot_id, steps);
                    }
                }
            }
        }
    };

    auto Shot::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Shot, World>(&Shot::Internals::advance),
        };
    }

}
