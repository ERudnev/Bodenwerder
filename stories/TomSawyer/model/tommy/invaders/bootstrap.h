#pragma once

#include <tommy/invaders/actors.h>
#include <tommy/invaders/combat.h>
#include <tommy/invaders/session.h>

#include <fQSM/api/interface.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    struct Bootstrap : Manipulation<Bootstrap, Session> {
        static auto newMatch(
            Writing,
            World::Id,
            rmmr::scene::Root::Id,
            rmmr::resource::sprite::Pack::Id,
            rmmr::resource::material::Asset::Id) -> Session::Id;

        static void installWave(Writing, Session::Id, integer wave);
        static void resetMatch(Writing, Session::Id);
        static void clearShots(Writing, Session::Id);
        static void clearAliens(Writing, Session::Id);
    };

}
