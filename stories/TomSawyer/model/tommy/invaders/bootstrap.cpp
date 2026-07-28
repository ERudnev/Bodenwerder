#include <tommy/invaders/bootstrap.h>

#include <tommy/invaders/visual.h>
#include <tommy/world.h>

#include <vector>

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
            const auto world = with<Session>::get(context, session).world;
            fleet->next_march = with<World>::get(context, world).step + Fleet::base_march_steps;

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
                    const auto body = createSomethingWithSprite(
                        context,
                        session_q,
                        pos,
                        Alien::sprite_index(kind),
                        Alien::sprite_scale,
                        Alien::sprite_bank,
                        Alien::sprite_zet);
                    with<Alien_group>::addElement(context, session, body, Alien::Quantum{
                        .cell = cell,
                        .kind = kind,
                        .points = points,
                        .alive = true,
                    });
                }
            }
        }

    } // namespace

    void Bootstrap::clearShots(Writing context, Session::Id session) {
        if (not with<Shot_group>::exists(context, session)) {
            return;
        }
        vector<Shot::Id> shots;
        for (const auto id : with<Shot_group>::get(context, session)) {
            shots.push_back(id);
        }
        for (const auto id : shots) {
            destroySomethingSprite(context, id);
            with<Shot_group>::deleteElement(context, session, id);
        }
    }

    void Bootstrap::clearAliens(Writing context, Session::Id session) {
        if (not with<Alien_group>::exists(context, session)) {
            return;
        }
        vector<Alien::Id> aliens;
        for (const auto id : with<Alien_group>::get(context, session)) {
            aliens.push_back(id);
        }
        for (const auto id : aliens) {
            destroySomethingSprite(context, id);
            with<Alien_group>::deleteElement(context, session, id);
        }
    }

    void Bootstrap::installWave(Writing context, Session::Id session, integer wave) {
        clearShots(context, session);
        clearAliens(context, session);
        spawn_fleet_aliens(context, session, wave);
        if (with<Volley>::exists(context, session)) {
            const auto world = with<Session>::get(context, session).world;
            with<Volley>::modify(context, session)->next_fire =
                with<World>::get(context, world).step + Volley::min_gap_steps;
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
        if (quantum->player and with<Player>::exists(context, *quantum->player)) {
            auto player = with<Player>::modify(context, *quantum->player);
            player->pos = index2{0, -380};
            player->cooldown_until = 0;
            syncSomethingSprite(context, *quantum->player, player->pos);
        }
        installWave(context, session, 1);
        syncMenuCameraControl(context, session);
    }

    auto Bootstrap::newMatch(
        Writing context,
        World::Id world,
        rmmr::scene::Root::Id scene,
        rmmr::scene::Camera::Id camera,
        rmmr::resource::sprite::Pack::Id pack,
        rmmr::resource::material::Asset::Id material) -> Session::Id
    {
        const auto session = with<Session>::create(context, Session::Quantum{
            .world = world,
            .phase = Phase::attract,
            .score = 0,
            .lives = 3,
            .wave = 1,
            .scene = scene,
            .camera = camera,
            .pack = pack,
            .material = material,
            .wave_ready_at = 0,
            .player = {},
        });

        with<Playfield>::extend(context, session, Playfield::Quantum{});
        with<Shot_group>::extend(context, session);
        with<Alien_group>::extend(context, session);

        with<Fleet>::extend(context, session, Fleet::Quantum{});
        with<Volley>::extend(context, session, Volley::Quantum{});

        const auto& session_q = with<Session>::get(context, session);
        const index2 player_pos{0, -380};
        const auto player_body = createSomethingWithSprite(
            context,
            session_q,
            player_pos,
            Player::sprite_idle,
            Player::sprite_scale,
            Player::sprite_bank,
            Player::sprite_zet);
        with<Player>::extend(context, player_body, Player::Quantum{
            .session = session,
            .pos = player_pos,
            .cooldown_until = 0,
        });
        with<Session>::modify(context, session)->player = player_body;

        installWave(context, session, 1);
        syncMenuCameraControl(context, session);
        return session;
    }

}
