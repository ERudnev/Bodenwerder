#pragma once

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>

namespace toy::ui {

    using namespace fqsm::api;

    struct State {
        bool materials = false;
        base::maybe<rmmr::scene::Root::Id> scene;
        base::maybe<rmmr::scene::Camera::Id> camera;

        void drawToggles(Writing);
        void drawCamera(Writing);
        void drawMaterials(Writing);
        void draw(Writing);
    };

}
