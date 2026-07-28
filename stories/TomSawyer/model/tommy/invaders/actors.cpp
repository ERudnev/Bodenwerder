#include <tommy/invaders/actors.h>

#include <tommy/invaders/combat.h>
#include <tommy/invaders/session.h>
#include <tommy/invaders/visual.h>
#include <tommy/world.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace tommy::invaders {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        auto key_down(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
        }

        auto player_body(Reading context, Session::Id session_id) -> base::maybe<GameObject::Id> {
            if (not with<Session>::exists(context, session_id)) {
                return {};
            }
            const auto player = with<Session>::get(context, session_id).player;
            if (not player or not with<Player>::exists(context, *player)) {
                return {};
            }
            return player;
        }

        auto march_interval(Reading context, Session::Id session, integer wave) -> integer {
            integer alive = 0;
            if (with<Alien_group>::exists(context, session)) {
                for (const auto id : with<Alien_group>::get(context, session)) {
                    if (with<GameObject>::alive(context, id)) {
                        ++alive;
                    }
                }
            }
            const integer base = std::max(Fleet::base_march_steps - (wave - 1) * 40, 120);
            if (alive <= 0) {
                return base;
            }
            return std::max(base * alive / 55, 80);
        }

    } // namespace

    auto Alien::worldPos(const Fleet::Quantum& fleet, index2 cell) -> index2 {
        return index2{
            fleet.origin.x + cell.x * Fleet::cell_size.x,
            fleet.origin.y - cell.y * Fleet::cell_size.y,
        };
    }

    auto Alien::sprite_index(Kind kind) -> integer {
        switch (kind) {
            case Kind::squid: return sprite_squid;
            case Kind::crab: return sprite_crab;
            case Kind::octopus: return sprite_octopus;
        }
        return sprite_crab;
    }

    auto Alien::sprite_tint(index2 cell) -> rmmr::RGB {
        // Additive tint (neutral is 0). Spread formation slots across a soft hue wheel.
        constexpr float two_pi = 6.2831853f;
        const float hue = (static_cast<float>(cell.x) * 0.55f + static_cast<float>(cell.y) * 1.7f) * (two_pi / 11.0f);
        constexpr float amp = 0.35f;
        return rmmr::RGB{
            amp * std::sin(hue),
            amp * std::sin(hue + 2.0943951f),
            amp * std::sin(hue + 4.1887902f),
        };
    }

    struct Player::Internals : Player::DefaultInternals {
        static void steer(
            Writing context,
            GameObject::Id player_id,
            Session::Id session_id,
            integer steps,
            const rmmr::system::Window::InputState& held)
        {
            auto player = with<Player>::modify(context, player_id);
            const auto& field = with<Playfield>::get(context, session_id);
            integer dx = 0;
            if (key_down(held.keys, GLFW_KEY_A) or key_down(held.keys, GLFW_KEY_LEFT)) {
                dx -= Player::move_pixels * steps;
            }
            if (key_down(held.keys, GLFW_KEY_D) or key_down(held.keys, GLFW_KEY_RIGHT)) {
                dx += Player::move_pixels * steps;
            }
            if (dx == 0) {
                return;
            }
            const integer min_x = field.origin.x + 40;
            const integer max_x = field.origin.x + field.size.x - 40;
            player->pos.x = std::clamp(player->pos.x + dx, min_x, max_x);
            syncGameObjectSprite(context, player_id, player->pos);
        }

        static void tryFire(
            Writing context,
            GameObject::Id player_id,
            Session::Id session_id,
            const rmmr::system::Window::InputState& held)
        {
            if (not key_down(held.keys, GLFW_KEY_SPACE) and not key_down(held.keys, GLFW_KEY_W)) {
                return;
            }
            const auto& player = with<Player>::get(context, player_id);
            const index2 muzzle{player.pos.x, player.pos.y + Gun::muzzle_lift};
            with<Gun>::fire(context, player.gun, session_id, muzzle);
        }

        static void onWorldStep(Reacting context) {
            const auto by_world = ask::relations<World>(context).updated<Session, &Session::Quantum::world>();
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step or change.now.paused) {
                    continue;
                }
                const integer steps = change.now.step - change.old.step;

                for (const auto entry : context.proposal.aspect<rmmr::system::Window>().items()) {
                    const auto& input = with<rmmr::system::Window>::get(context, entry.id);

                    for (const auto session_id : by_world.ids(change.id)) {
                        if (not sessionPlaying(context, session_id)) {
                            continue;
                        }
                        const auto player_id = player_body(context, session_id);
                        if (not player_id) {
                            continue;
                        }
                        steer(context, *player_id, session_id, steps, input.current);
                        tryFire(context, *player_id, session_id, input.current);
                    }
                }
            }
        }
    };

    auto Player::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Player, Gun, &Player::Quantum::gun>{},
            reaction::aspect_wide<Player, World>(&Player::Internals::onWorldStep),
        };
    }

    struct Fleet::Internals : Fleet::DefaultInternals {
        static void march(Reacting context) {
            const auto by_world = ask::relations<World>(context).updated<Session, &Session::Quantum::world>();
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step or change.now.paused) {
                    continue;
                }

                for (const auto session_id : by_world.ids(change.id)) {
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
                    const integer lateral = fleet->dir == Fleet::Dir::right
                        ? Fleet::lateral_step
                        : -Fleet::lateral_step;
                    bool hit_edge = false;
                    if (with<Alien_group>::exists(context, session_id)) {
                        for (const auto alien_id : with<Alien_group>::get(context, session_id)) {
                            const auto& alien = with<Alien>::get(context, alien_id);
                            if (not with<GameObject>::alive(context, alien_id)) {
                                continue;
                            }
                            const index2 next = Alien::worldPos(*fleet, alien.cell);
                            const integer x = next.x + lateral;
                            if (x < field.origin.x + Fleet::edge_margin
                                or x > field.origin.x + field.size.x - Fleet::edge_margin)
                            {
                                hit_edge = true;
                                break;
                            }
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

                    if (with<Alien_group>::exists(context, session_id)) {
                        for (const auto alien_id : with<Alien_group>::get(context, session_id)) {
                            if (not with<GameObject>::alive(context, alien_id)) {
                                continue;
                            }
                            const auto& alien = with<Alien>::get(context, alien_id);
                            const index2 pos = Alien::worldPos(*fleet, alien.cell);
                            syncGameObjectSprite(context, alien_id, pos);
                            const auto player_id = player_body(context, session_id);
                            if (player_id and pos.y <= with<Player>::get(context, *player_id).pos.y + Fleet::edge_margin) {
                                with<Session>::modify(context, session_id)->phase = Phase::lost;
                            }
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
            const auto by_world = ask::relations<World>(context).updated<Session, &Session::Quantum::world>();
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step or change.now.paused) {
                    continue;
                }

                for (const auto session_id : by_world.ids(change.id)) {
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
                    if (with<Alien_group>::exists(context, session_id)) {
                        for (const auto alien_id : with<Alien_group>::get(context, session_id)) {
                            if (with<GameObject>::alive(context, alien_id)) {
                                candidates.push_back(alien_id);
                            }
                        }
                    }
                    if (candidates.empty()) {
                        continue;
                    }

                    const auto pick = candidates[static_cast<std::size_t>(change.now.step) % candidates.size()];
                    const auto& fleet = with<Fleet>::get(context, session_id);
                    const auto& alien = with<Alien>::get(context, pick);
                    const index2 cell_pos = Alien::worldPos(fleet, alien.cell);
                    const index2 muzzle{cell_pos.x, cell_pos.y - Volley::muzzle_drop};
                    with<Volley>::fire(context, session_id, muzzle);
                    volley->next_fire = change.now.step + Volley::min_gap_steps;
                }
            }
        }
    };

    auto Volley::Actions::fire(Writing context, Id session_id, index2 muzzle) -> GameObject::Id {
        const auto& session = with<Session>::get(context, session_id);
        const auto body = createGameObjectWithSprite(
            context,
            session,
            muzzle,
            Shot::sprite_index(Shot::Side::alien),
            Shot::sprite_scale,
            Shot::sprite_bank,
            Shot::sprite_zet,
            rmmr::RGB{0.0f, 0.0f, 0.0f},
            Shot::max_hitpoints);
        with<Shot_group>::addElement(context, session_id, body, Shot::Quantum{
            .session = session_id,
            .pos = muzzle,
            .side = Shot::Side::alien,
        });
        return body;
    }

    auto Volley::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Volley, World>(&Volley::Internals::schedule),
        };
    }

}
