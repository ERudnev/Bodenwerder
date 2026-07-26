#include <tommy/invaders/combat.h>

#include <tommy/invaders/actors.h>
#include <tommy/invaders/session.h>
#include <tommy/invaders/visual.h>
#include <tommy/world.h>

#include <vector>

namespace tommy::invaders {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        constexpr index2 k_shot_half{6, 20};
        constexpr index2 k_player_half{40, 28};
        constexpr index2 k_alien_half{36, 30};

    } // namespace

    struct Shot::Internals : Shot::DefaultInternals {
        static void advance(Reacting context) {
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step or change.now.paused) {
                    continue;
                }
                const integer steps = change.now.step - change.old.step;

                vector<Shot::Id> doomed;
                for (const auto [shot_id, _] : context.proposal.aspect<Shot>().items()) {
                    auto shot = with<Shot>::modify(context, shot_id);
                    const Session::Id session = shot->session;
                    if (not with<Session>::exists(context, session)) {
                        doomed.push_back(shot_id);
                        continue;
                    }
                    if (not sessionPlaying(context, session)) {
                        continue;
                    }

                    const integer speed = shot->side == Side::player ? player_speed : -alien_speed;
                    shot->pos.y += speed * steps;
                    syncVisual(context, shot->visual, shot->pos);

                    const auto& field = with<Playfield>::get(context, session);
                    if (shot->pos.y > field.origin.y + field.size.y + 40
                        or shot->pos.y < field.origin.y - 40)
                    {
                        doomed.push_back(shot_id);
                        continue;
                    }

                    if (shot->side == Side::alien
                        and with<Player>::exists(context, session))
                    {
                        const auto& player = with<Player>::get(context, session);
                        if (aabbOverlap(shot->pos, k_shot_half, player.pos, k_player_half)) {
                            notePlayerHit(context, session);
                            doomed.push_back(shot_id);
                            continue;
                        }
                    }

                    if (shot->side == Side::player
                        and with<Fleet>::exists(context, session))
                    {
                        const auto& fleet = with<Fleet>::get(context, session);
                        for (const auto [alien_id, _] : context.proposal.aspect<Alien>().items()) {
                            auto alien = with<Alien>::modify(context, alien_id);
                            if (not alien->alive) {
                                continue;
                            }
                            const index2 pos = alienWorldPos(fleet, alien->cell);
                            if (not aabbOverlap(shot->pos, k_shot_half, pos, k_alien_half)) {
                                continue;
                            }
                            alien->alive = false;
                            destroyVisual(context, alien->visual);
                            with<Session>::modify(context, session)->score += alien->points;
                            doomed.push_back(shot_id);

                            bool any_alive = false;
                            for (const auto [id, _] : context.proposal.aspect<Alien>().items()) {
                                if (with<Alien>::get(context, id).alive) {
                                    any_alive = true;
                                    break;
                                }
                            }
                            if (not any_alive) {
                                noteFleetCleared(context, session);
                            }
                            break;
                        }
                    }
                }

                for (const auto shot_id : doomed) {
                    if (not with<Shot>::exists(context, shot_id)) {
                        continue;
                    }
                    destroyVisual(context, with<Shot>::get(context, shot_id).visual);
                    with<Shot>::remove(context, shot_id);
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
