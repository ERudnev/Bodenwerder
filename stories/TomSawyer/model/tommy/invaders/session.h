#pragma once

#include <cstdint>

#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/root.q1.h>
#include <tommy/world.h>

#include <fQSM/api/interface.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    // Kenney sheet.xml order (spaceShooter).
    inline constexpr integer k_sprite_player = 200; // playerShip1_blue
    inline constexpr integer k_sprite_alien_squid = 49; // enemyBlack1
    inline constexpr integer k_sprite_alien_crab = 50; // enemyBlack2
    inline constexpr integer k_sprite_alien_octopus = 51; // enemyBlack3
    inline constexpr integer k_sprite_laser_player = 105; // laserBlue01
    inline constexpr integer k_sprite_laser_alien = 137; // laserRed01

    enum class Phase : std::uint8_t {
        attract,
        playing,
        wave_clear,
        lost,
        won,
    };

    struct Session : Entity<Session> {
        struct Quantum {
            Affected<World> world;
            Phase phase = Phase::attract;
            integer score = 0;
            integer lives = 3;
            integer wave = 1;
            rmmr::scene::Root::Id scene;
            rmmr::resource::sprite::Pack::Id pack;
            rmmr::resource::material::Asset::Id material;
            integer wave_ready_at = 0;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Playfield : Component<Playfield, Session> {
        struct Quantum {
            index2 origin{-800, -450};
            index2 size{1600, 900};
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    void noteFleetCleared(Writing, Session::Id);
    void notePlayerHit(Writing, Session::Id);
    auto sessionPlaying(Reading, Session::Id) -> bool;

}
