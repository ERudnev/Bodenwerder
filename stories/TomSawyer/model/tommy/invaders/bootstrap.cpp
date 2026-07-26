#include <tommy/invaders/bootstrap.h>

#include <tommy/invaders/visual.h>
#include <tommy/world.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        void spawn_fleet_aliens(Writing context, Session::Id session, integer /*wave*/) {
            if (not with<Alien_group>::exists(context, session)) {
                with<Alien_group>::extend(context, session);
            }
            auto fleet = with<Fleet>::modify(context, session);
            fleet->origin = index2{-350, 260};
            fleet->dir = Fleet::Dir::right;
            for (const auto [id, _] : context->aspect<World>().items()) {
                fleet->next_march = with<World>::get(context, id).step + Fleet::base_march_steps;
                break;
            }

            const auto& session_q = with<Session>::get(context, session);
            constexpr integer cols = 11;
            constexpr integer rows = 5;
            for (integer row = 0; row < rows; ++row) {
                Alien::Kind kind = Alien::Kind::octopus;
                integer points = 10;
                if (row == 0) {
                    kind = Alien::Kind::squid;
                    points = 30;
                } else if (row <= 2) {
                    kind = Alien::Kind::crab;
                    points = 20;
                }
                for (integer col = 0; col < cols; ++col) {
                    const index2 cell{col, row};
                    const index2 pos = alienWorldPos(*fleet, cell);
                    const auto visual = spawnSprite(context, session_q, pos, alienSpriteIndex(kind), 0);
                    with<Alien_group>::addElement(context, session, Alien::Quantum{
                        .cell = cell,
                        .kind = kind,
                        .points = points,
                        .alive = true,
                        .visual = visual,
                    });
                }
            }
        }

    } // namespace

    void Bootstrap::clearShots(Writing context, Session::Id session) {
        vector<Shot::Id> shots;
        for (const auto [id, _] : context->aspect<Shot>().items()) {
            if (with<Shot>::get(context, id).session == session) {
                shots.push_back(id);
            }
        }
        for (const auto id : shots) {
            destroyVisual(context, with<Shot>::get(context, id).visual);
            with<Shot>::remove(context, id);
        }
    }

    void Bootstrap::clearAliens(Writing context, Session::Id session) {
        vector<Alien::Id> aliens;
        for (const auto [id, _] : context->aspect<Alien>().items()) {
            aliens.push_back(id);
        }
        for (const auto id : aliens) {
            destroyVisual(context, with<Alien>::get(context, id).visual);
            with<Alien>::remove(context, id);
        }
        (void)session;
    }

    void Bootstrap::installWave(Writing context, Session::Id session, integer wave) {
        clearShots(context, session);
        clearAliens(context, session);
        spawn_fleet_aliens(context, session, wave);
        if (with<Volley>::exists(context, session)) {
            for (const auto [id, _] : context->aspect<World>().items()) {
                with<Volley>::modify(context, session)->next_fire =
                    with<World>::get(context, id).step + Volley::min_gap_steps;
                break;
            }
        }
    }

    void Bootstrap::resetMatch(Writing context, Session::Id session) {
        clearShots(context, session);
        clearAliens(context, session);
        auto quantum = with<Session>::modify(context, session);
        quantum->phase = Phase::playing;
        quantum->score = 0;
        quantum->lives = 3;
        quantum->wave = 1;
        quantum->wave_ready_at = 0;
        if (with<Player>::exists(context, session)) {
            auto player = with<Player>::modify(context, session);
            player->pos = index2{0, -380};
            player->cooldown_until = 0;
            syncVisual(context, player->visual, player->pos);
        }
        installWave(context, session, 1);
    }

    auto Bootstrap::newMatch(
        Writing context,
        rmmr::scene::Root::Id scene,
        rmmr::resource::sprite::Pack::Id pack,
        rmmr::resource::material::Asset::Id material) -> Session::Id
    {
        const auto session = with<Session>::create(context, Session::Quantum{
            .phase = Phase::attract,
            .score = 0,
            .lives = 3,
            .wave = 1,
            .scene = scene,
            .pack = pack,
            .material = material,
            .wave_ready_at = 0,
        });

        with<Playfield>::extend(context, session, Playfield::Quantum{});
        with<Shot_group>::extend(context, session);
        with<Alien_group>::extend(context, session);

        with<Fleet>::extend(context, session, Fleet::Quantum{});
        with<Volley>::extend(context, session, Volley::Quantum{});

        const auto& session_q = with<Session>::get(context, session);
        const index2 player_pos{0, -380};
        const auto player_visual = spawnSprite(context, session_q, player_pos, k_sprite_player, 2);
        with<Player>::extend(context, session, Player::Quantum{
            .pos = player_pos,
            .cooldown_until = 0,
            .visual = player_visual,
        });

        installWave(context, session, 1);
        return session;
    }

}
