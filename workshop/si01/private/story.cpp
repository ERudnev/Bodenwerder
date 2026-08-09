#include "story.h"

#include <rmmr/api/_interface.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <si01/invaders/actors.h>
#include <si01/invaders/bootstrap.h>
#include <si01/invaders/combat.h>
#include <si01/invaders/session.h>
#include <si01/world.h>

namespace si01 {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema SpriteTest::schema() const {
        return ask::schema::merge({
            ask::schema::aspect<World>(),
            ask::schema::aspect<invaders::GameObject>(),
            ask::schema::aspect<invaders::Session>(),
            ask::schema::aspect<invaders::Playfield>(),
            ask::schema::aspect<invaders::Gun>(),
            ask::schema::aspect<invaders::Player>(),
            ask::schema::aspect<invaders::Fleet>(),
            ask::schema::aspect<invaders::Alien>(),
            ask::schema::aspect<invaders::Alien_group>(),
            ask::schema::aspect<invaders::Volley>(),
            ask::schema::aspect<invaders::Shot>(),
            ask::schema::aspect<invaders::Shot_group>(),
        });
    }

    void SpriteTest::setup(Writing context, system::Window::Id window) {
        const auto world = with<World>::create(context, World::Quantum{.step = 0, .paused = false});

        const auto framebuffer = with<system::Window>::framebufferSize(context, window);
        const auto viewport = with<system::Viewport_group>::addElement(context, window, system::Viewport::Quantum{
            .origin = index2{0, 0},
            .size = framebuffer,
            .clear_color = vec4{0.05f, 0.05f, 0.12f, 1.0f},
        });

        const auto root = with<scene::Interface>::createScene(context);
        with<scene::Flat2d>::extend(context, root, scene::Flat2d::Quantum{
            .size = index2{1600, 900},
            .device = window,
        });
        with<scene::actor::Sprite>::modify_global(context)->geometry = assets->unitQuad;

        const auto camera = with<scene::Flat2d>::createCamera(context, root,
            Pose::from(Pos{0.0f, 0.0f, 5.0f}, HPB{0.0f, 0.0f, 0.0f}));

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

    void SpriteTest::onFrame(establish::Realm&, int64) {
        // Sim advances via World reactions on system::Clock (beginFrame).
    }

}
