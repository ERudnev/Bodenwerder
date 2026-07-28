#include "story.h"

#include <rmmr/api/_interface.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <tommy/invaders/actors.h>
#include <tommy/invaders/bootstrap.h>
#include <tommy/invaders/combat.h>
#include <tommy/invaders/session.h>
#include <tommy/world.h>

namespace tommy {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema SpriteTest::schema() const {
        return ask::schema::merge({
            ask::schema::aspect<World>(),
            ask::schema::aspect<invaders::Something>(),
            ask::schema::aspect<invaders::Session>(),
            ask::schema::aspect<invaders::Playfield>(),
            ask::schema::aspect<invaders::Player>(),
            ask::schema::aspect<invaders::Fleet>(),
            ask::schema::aspect<invaders::Alien>(),
            ask::schema::aspect<invaders::Alien_group>(),
            ask::schema::aspect<invaders::Volley>(),
            ask::schema::aspect<invaders::Shot>(),
            ask::schema::aspect<invaders::Shot_group>(),
        });
    }

    void SpriteTest::setup(Writing context, system::Core::Id, system::Viewport::Id viewport) {
        const auto world = with<World>::create(context, World::Quantum{.step = 0, .paused = false});

        const auto root = with<scene::Interface>::createScene(context);
        with<scene::Flat2d>::extend(context, root, scene::Flat2d::Quantum{
            .size = index2{1600, 900},
        });
        with<scene::actor::Sprite>::modify_global(context)->geometry = assets->unitQuad;

        const auto camera = with<scene::Flat2d>::createCamera(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 5.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}});

        invaders::Bootstrap::newMatch(
            context,
            world,
            root,
            camera,
            assets->kenney,
            *shared->material.sprite);

        views = {
            View{.viewport = viewport, .scene = root, .camera = camera},
        };
    }

}
