#pragma once

#include <cstdint>

#include <rmmr/scene/node.q1.h>
#include <tommy/invaders/session.h>

#include <fQSM/api/interface.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    struct Player : Component<Player, Session> {
        static constexpr integer move_pixels = 2;
        static constexpr integer shot_cooldown_steps = 200;
        struct Quantum {
            index2 pos{0, -380};
            integer cooldown_until = 0;
            rmmr::scene::Node::Id visual;
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

    struct Alien : Entity<Alien> {
        enum class Kind : std::uint8_t { squid, crab, octopus };
        struct Quantum {
            index2 cell{0, 0};
            Kind kind = Kind::crab;
            integer points = 10;
            bool alive = true;
            rmmr::scene::Node::Id visual;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
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
    auto alienSpriteIndex(Alien::Kind kind) -> integer;

}
