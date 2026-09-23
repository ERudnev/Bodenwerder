#include "strategic/map.h"

#include "geo/celestial/horizon.h"

#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/window.q1.h>

#include <cmath>
#include <numbers>

namespace eltanin::strategic {

    using namespace fqsm::api;
    using namespace rmmr;

    void Map::open(Writing context, system::Window::Id window) {
        const auto framebuffer = with<system::Window>::framebufferSize(context, window);
        const auto viewport = with<system::Viewport_group>::addElement(context, window, system::Viewport::Quantum{
            .origin = index2{0, 0},
            .size = framebuffer,
            .clear_color = vec4{0.0f, 0.0f, 0.0f, 1.0f},
        });
        const auto root = with<scene::Interface>::createScene(context);
        const auto eye = with<scene::Interface>::createCamera(context, root, Pose::from(Pos{0.0f, 0.0f, 400.0f}, HPB{0.0f, 0.0f, 0.0f}), 60.0f * std::numbers::pi_v<float> / 180.0f);
        {
            auto quantum = with<scene::Camera>::modify(context, eye);
            quantum->z_near = geo::Horizon::near;
            quantum->z_far = geo::Horizon::far;
        }
        with<controller::Camera3d>::create(context, eye);
        scene = root;
        camera = eye;
        view = rmmr::wrapper::Product::View{.viewport = viewport, .scene = root, .camera = eye};
    }

}
