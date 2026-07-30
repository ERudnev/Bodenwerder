#include "story.h"

#include <kubes/world.h>
#include <rmmr/api/_interface.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>

namespace kubes {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema KubeOfKubes::schema() const {
        return ask::schema::aspect<World>();
    }

    void KubeOfKubes::addAssets(Writing context, system::Core::Id core) {
        using namespace resource;
        using geometry::Generator;

        assets.primitive.grid = with<::rmmr::resource::Assets>::add_geometry_generator(
            context, core,
            item<Unit>{.manager = core, .name = "grid", .library = "rmmr"},
            item<Generator>{.type = Generator::Type::gridPlane});
    }

    void KubeOfKubes::populateWorld(Writing context, system::Window::Id window) {
        with<World>::modify_global(context)->window = window;

        const auto framebuffer = with<system::Window>::framebufferSize(context, window);
        const auto viewport = with<system::Viewport_group>::addElement(context, window, system::Viewport::Quantum{
            .origin = index2{0, 0},
            .size = framebuffer,
            .clear_color = vec4{0.2f, 0.3f, 0.3f, 1.0f},
        });

        const auto root = with<scene::Interface>::createScene(context);

        with<scene::Interface>::createGrid(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 0.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Grid>{.geometry = *assets.primitive.grid, .material = *shared->material.grid, .opacity = 1.0f});

        const auto camera = with<scene::Interface>::createCamera(context, root,
            Locator{.pos = Pos{10.5f, 10.0f, 14.0f}, .euler = HPB{36.87f, -29.74f, 0.0f}},
            1.04719755f);
        with<controller::Camera3d>::create(context, camera);
        with<scene::Interface>::createLight(context, root,
            Locator{.pos = Pos{9.5f, 19.0f, 7.5f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 30.0f});

        views = {
            View{.viewport = viewport, .scene = root, .camera = camera},
        };
    }

    void KubeOfKubes::setup(establish::Realm& world, system::Core::Id, system::Window::Id window) {
        populateWorld(world, window);
    }

}
