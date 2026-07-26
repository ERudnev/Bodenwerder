#pragma once

#include <cstdint>

#include <rmmr/scene/node.q1.h>
#include <tommy/invaders/session.h>

#include <fQSM/api/interface.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    struct Shot : Entity<Shot> {
        enum class Side : std::uint8_t { player, alien };
        static constexpr integer player_speed = 1; // px per World.step
        static constexpr integer alien_speed = 1;
        struct Quantum {
            Affected<Session> session;
            index2 pos{0, 0};
            Side side = Side::player;
            rmmr::scene::Node::Id visual;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Shot_group : Group<Shot_group, Session, Shot> {
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
