#pragma once

#include <cstdint>

#include <rmmr/math.q1.h>
#include <tommy/invaders/gun.h>
#include <tommy/invaders/session.h>

#include <fQSM/api/interface.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    struct Player : Feature<Player, GameObject> {
        static constexpr integer move_pixels = 2;
        static constexpr integer sprite_idle = 200; // Kenney playerShip1_blue
        static constexpr float sprite_scale = 0.5f;
        static constexpr float sprite_bank = 180.0f; // Node HPB.z degrees
        static constexpr integer sprite_zet = 2;
        struct Quantum {
            Affected<Session> session;
            index2 pos{0, -380};
            Custody<Gun> gun;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Fleet : Component<Fleet, Session> {
        enum class Dir : std::uint8_t { left, right };
        static constexpr integer descend_pixels = 24;
        static constexpr integer base_march_steps = 400;
        static constexpr index2 cell_size{70, 56};
        struct Quantum {
            index2 origin{-350, 260};
            Dir dir = Dir::right;
            integer next_march = 0;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Alien : Feature<Alien, GameObject> {
        enum class Kind : std::uint8_t { squid, crab, octopus };
        static constexpr integer sprite_squid = 49; // Kenney enemyBlack1
        static constexpr integer sprite_crab = 50;
        static constexpr integer sprite_octopus = 51;
        static constexpr float sprite_scale = 0.5f;
        static constexpr float sprite_bank = 180.0f; // Node HPB.z degrees
        static constexpr integer sprite_zet = 0;
        struct Quantum {
            index2 cell{0, 0};
            Kind kind = Kind::crab;
            integer points = 10;
            bool alive = true;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
        static auto sprite_index(Kind kind) -> integer;
        static auto sprite_tint(index2 cell) -> rmmr::RGB;
    };

    struct Alien_group : Group<Alien_group, Fleet, Alien> {
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Volley : Attribute<Volley, Fleet> {
        static constexpr integer min_gap_steps = 500;
        struct Quantum {
            integer next_fire = 0;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    auto alienWorldPos(const Fleet::Quantum& fleet, index2 cell) -> index2;

}
