#pragma once

#include <cstdint>

#include <si01/invaders/session.h>

#include <fQSM/api/interface.h>

namespace si01::invaders {

    using namespace fqsm::api;

    struct Shot : Feature<Shot, GameObject> {
        enum class Side : std::uint8_t { player, alien };
        static constexpr integer max_hitpoints = 1;
        static constexpr integer player_speed = 1; // px per World.step
        static constexpr integer alien_speed = 1; // px per 2 World.steps (half prior rate)
        static constexpr integer alien_speed_period = 2;
        static constexpr integer sprite_player = 105; // Kenney laserBlue01
        static constexpr integer sprite_alien = 137; // Kenney laserRed01
        static constexpr float sprite_scale = 0.5f;
        static constexpr float sprite_bank = 0.0f;
        static constexpr integer sprite_zet = 1;
        static constexpr index2 hit_half{3, 10};
        struct Quantum {
            Affected<Session> session;
            index2 pos{0, 0};
            Side side = Side::player;
            integer motion_carry = 0; // alien half-speed remainder
        };
        struct Internals;
        static const Behavior customAspectReactions();
        static auto sprite_index(Side side) -> integer;
    };

    struct Shot_group : Group<Shot_group, Session, Shot> {
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
