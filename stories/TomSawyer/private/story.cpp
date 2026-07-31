#include "story.h"

#include <rmmr/api/_interface.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <tommy/world.q1.h>

namespace tommy {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema SpriteTest::schema() const {
        return ask::schema::aspect<World>();
    }

    void SpriteTest::populateWorld(Writing context, system::Window::Id window) {
        with<World>::create(context, World::Quantum{.step = 0, .paused = false});

        const auto framebuffer = with<system::Window>::framebufferSize(context, window);
        const auto viewport = with<system::Viewport_group>::addElement(context, window, system::Viewport::Quantum{
            .origin = index2{0, 0},
            .size = framebuffer,
            .clear_color = vec4{0.2f, 0.3f, 0.3f, 1.0f},
        });

        const auto root = with<scene::Interface>::createScene(context);
        with<scene::Flat2d>::extend(context, root, scene::Flat2d::Quantum{
            .size = index2{1600, 900},
        });
        with<scene::actor::Sprite>::modify_global(context)->geometry = assets->unitQuad;

        const auto camera = with<scene::Flat2d>::createCamera(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 5.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}});

        views = {
            View{.viewport = viewport, .scene = root, .camera = camera},
        };
    }

    void SpriteTest::setup(establish::Realm& world, system::Window::Id window) {
        populateWorld(world, window);
    }

    void SpriteTest::advanceSim(Writing context, int64 dt_us) {
        with<World>::advance(context, dt_us);
    }

    void SpriteTest::onFrame(establish::Realm& world, int64 dt_us) {
        advanceSim(world, dt_us);
    }

}
