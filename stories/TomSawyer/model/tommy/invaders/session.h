#pragma once

#include <cstdint>

#include <base/maybe.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <tommy/invaders/gameObject.h>
#include <tommy/world.h>

#include <fQSM/api/interface.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    enum class Phase : std::uint8_t {
        attract,
        playing,
        wave_clear,
        lost,
        won,
    };

    struct Session : Entity<Session> {
        static constexpr integer start_lives = 3;
        static constexpr integer wave_clear_steps = 800;
        struct Quantum {
            Anchor<World> world;
            Phase phase = Phase::attract;
            integer score = 0;
            integer lives = start_lives;
            integer wave = 1;
            rmmr::scene::Root::Id scene;
            rmmr::scene::Camera::Id camera;
            rmmr::resource::sprite::Pack::Id pack;
            rmmr::resource::material::Asset::Id material;
            integer wave_ready_at = 0;
            base::maybe<GameObject::Id> player;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Playfield : Component<Playfield, Session> {
        struct Quantum {
            index2 origin{-800, -450};
            index2 size{1600, 900};
        };
        struct Actions : BaseActions {
            static auto contains(Reading, Id, index2) -> bool;
            static void install(Writing, Id, index2 origin, index2 size);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    void noteFleetCleared(Writing, Session::Id);
    void notePlayerHit(Writing, Session::Id);
    void syncMenuCameraControl(Writing, Session::Id);
    auto sessionPlaying(Reading, Session::Id) -> bool;

}
