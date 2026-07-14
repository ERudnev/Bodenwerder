#pragma once

#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::ui {

    using namespace fqsm::api;

    struct FrameContext {
        fqsm::Writing world;
        system::Window::Id window;
        scene::Root::Id scene;
        scene::Camera::Id camera;
    };

    void draw_camera(FrameContext args);
    void draw_cubes(FrameContext args);

}
