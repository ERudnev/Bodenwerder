#include <tommy/invaders/actors.h>

#include <tommy/invaders/combat.h>
#include <tommy/invaders/session.h>
#include <tommy/invaders/visual.h>
#include <tommy/world.h>

#include <GLFW/glfw3.h>

#include <algorithm>

namespace tommy::invaders {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        auto key_down(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
        }

        auto key_edge(const rmmr::system::Window::InputState& previous, const rmmr::system::Window::InputState& current, int key) -> bool {
            return key_down(current.keys, key) and not key_down(previous.keys, key);
        }

        auto player_shot_alive(Reading context, Session::Id session) -> bool {
            for (const auto [id, _] : context->aspect<Shot>().items()) {
                const auto& shot = with<Shot>::get(context, id);
                if (shot.session == session and shot.side == Shot::Side::player) {
                    return true;
                }
            }
            return false;
        }

        auto march_interval(Reading context, Session::Id session, integer wave) -> integer {
            integer alive = 0;
            for (const auto [id, _] : context->aspect<Alien>().items()) {
                if (with<Alien>::get(context, id).alive) {
                    ++alive;
                }
            }
            const integer base = std::max(Fleet::base_march_steps - (wave - 1) * 40, 120);
            if (alive <= 0) {
                return base;
            }
            return std::max(base * alive / 55, 80);
        }

    } // namespace

    auto alienWorldPos(const Fleet::Quantum& fleet, index2 cell) -> index2 {
        return index2{
            fleet.origin.x + cell.x * Fleet::cell_size.x,
            fleet.origin.y - cell.y * Fleet::cell_size.y,
        };
    }

    auto alienSpriteIndex(Alien::Kind kind) -> integer {
        switch (kind) {
            case Alien::Kind::squid: return k_sprite_alien_squid;
            case Alien::Kind::crab: return k_sprite_alien_crab;
            case Alien::Kind::octopus: return k_sprite_alien_octopus;
        }
        return k_sprite_alien_crab;
    }

    struct Player::Internals : Player::DefaultInternals {
        static void steerAndFire(Reacting context) {
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step or change.now.paused) {
                    continue;
                }
                const integer steps = change.now.step - change.old.step;

                for (const auto entry : context.proposal.aspect<rmmr::system::Window>().items()) {
                    const auto& input = with<rmmr::system::Window>::get(context, entry.id);

                    for (const auto [session_id, _] : context.proposal.aspect<Session>().items()) {
                        if (not sessionPlaying(context, session_id)) {
                            continue;
                        }
                        if (not with<Player>::exists(context, session_id)) {
                            continue;
                        }

                        auto player = with<Player>::modify(context, session_id);
                        const auto& field = with<Playfield>::get(context, session_id);
                        integer dx = 0;
                        if (key_down(input.current.keys, GLFW_KEY_A) or key_down(input.current.keys, GLFW_KEY_LEFT)) {
                            dx -= Player::move_pixels * steps;
                        }
                        if (key_down(input.current.keys, GLFW_KEY_D) or key_down(input.current.keys, GLFW_KEY_RIGHT)) {
                            dx += Player::move_pixels * steps;
                        }
                        if (dx != 0) {
                            const integer min_x = field.origin.x + 40;
                            const integer max_x = field.origin.x + field.size.x - 40;
                            player->pos.x = std::clamp(player->pos.x + dx, min_x, max_x);
                            syncVisual(context, player->visual, player->pos);
                        }

                        const bool fire = key_edge(input.previous, input.current, GLFW_KEY_SPACE)
                            or key_edge(input.previous, input.current, GLFW_KEY_W);
                        if (fire
                            and change.now.step >= player->cooldown_until
                            and not player_shot_alive(context, session_id))
                        {
                            const auto& session = with<Session>::get(context, session_id);
                            const index2 muzzle{player->pos.x, player->pos.y + 40};
                            const auto visual = spawnSprite(context, session, muzzle, k_sprite_laser_player, 1);
                            with<Shot_group>::addElement(context, session_id, Shot::Quantum{
                                .session = session_id,
                                .pos = muzzle,
                                .side = Shot::Side::player,
                                .visual = visual,
                            });
                            player->cooldown_until = change.now.step + Player::shot_cooldown_steps;
                        }
                    }
                }
            }
        }
    };

    auto Player::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Player, World>(&Player::Internals::steerAndFire),
        };
    }

    struct Fleet::Internals : Fleet::DefaultInternals {
        static void march(Reacting context) {
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step or change.now.paused) {
                    continue;
                }

                for (const auto [session_id, _] : context.proposal.aspect<Session>().items()) {
                    if (not sessionPlaying(context, session_id)) {
                        continue;
                    }
                    if (not with<Fleet>::exists(context, session_id)) {
                        continue;
                    }

                    auto fleet = with<Fleet>::modify(context, session_id);
                    if (change.now.step < fleet->next_march) {
                        continue;
                    }

                    const auto& field = with<Playfield>::get(context, session_id);
                    const integer lateral = fleet->dir == Fleet::Dir::right ? 12 : -12;
                    bool hit_edge = false;
                    for (const auto [alien_id, _] : context.proposal.aspect<Alien>().items()) {
                        const auto& alien = with<Alien>::get(context, alien_id);
                        if (not alien.alive) {
                            continue;
                        }
                        const index2 next = alienWorldPos(*fleet, alien.cell);
                        const integer x = next.x + lateral;
                        if (x < field.origin.x + 40 or x > field.origin.x + field.size.x - 40) {
                            hit_edge = true;
                            break;
                        }
                    }

                    if (hit_edge) {
                        fleet->dir = fleet->dir == Fleet::Dir::right ? Fleet::Dir::left : Fleet::Dir::right;
                        fleet->origin.y -= Fleet::descend_pixels;
                    } else {
                        fleet->origin.x += lateral;
                    }

                    const auto& session = with<Session>::get(context, session_id);
                    fleet->next_march = change.now.step + march_interval(context, session_id, session.wave);

                    for (const auto [alien_id, _] : context.proposal.aspect<Alien>().items()) {
                        auto alien = with<Alien>::modify(context, alien_id);
                        if (not alien->alive) {
                            continue;
                        }
                        const index2 pos = alienWorldPos(*fleet, alien->cell);
                        syncVisual(context, alien->visual, pos);
                        if (with<Player>::exists(context, session_id)
                            and pos.y <= with<Player>::get(context, session_id).pos.y + 40)
                        {
                            with<Session>::modify(context, session_id)->phase = Phase::lost;
                        }
                    }
                }
            }
        }
    };

    auto Fleet::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Fleet, World>(&Fleet::Internals::march),
        };
    }

    struct Volley::Internals : Volley::DefaultInternals {
        static void schedule(Reacting context) {
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step or change.now.paused) {
                    continue;
                }

                for (const auto [session_id, _] : context.proposal.aspect<Session>().items()) {
                    if (not sessionPlaying(context, session_id)) {
                        continue;
                    }
                    if (not with<Volley>::exists(context, session_id)) {
                        continue;
                    }
                    auto volley = with<Volley>::modify(context, session_id);
                    if (change.now.step < volley->next_fire) {
                        continue;
                    }

                    vector<Alien::Id> candidates;
                    for (const auto [alien_id, _] : context.proposal.aspect<Alien>().items()) {
                        if (with<Alien>::get(context, alien_id).alive) {
                            candidates.push_back(alien_id);
                        }
                    }
                    if (candidates.empty()) {
                        continue;
                    }

                    const auto pick = candidates[static_cast<std::size_t>(change.now.step) % candidates.size()];
                    const auto& fleet = with<Fleet>::get(context, session_id);
                    const auto& alien = with<Alien>::get(context, pick);
                    const index2 muzzle = alienWorldPos(fleet, alien.cell);
                    const index2 spawn{muzzle.x, muzzle.y - 30};
                    const auto& session = with<Session>::get(context, session_id);
                    const auto visual = spawnSprite(context, session, spawn, k_sprite_laser_alien, 1);
                    with<Shot_group>::addElement(context, session_id, Shot::Quantum{
                        .session = session_id,
                        .pos = spawn,
                        .side = Shot::Side::alien,
                        .visual = visual,
                    });
                    volley->next_fire = change.now.step + Volley::min_gap_steps;
                }
            }
        }
    };

    auto Volley::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Volley, World>(&Volley::Internals::schedule),
        };
    }

}
