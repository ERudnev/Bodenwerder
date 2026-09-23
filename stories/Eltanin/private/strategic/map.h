#pragma once

#include <base/maybe.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/wrapper/product.h>

#include <fQSM/api/interface.h>

namespace eltanin::strategic {

    using namespace fqsm::api;

    struct Map {
        struct Menu {
            struct Blueprints {};
            base::maybe<Blueprints> blueprints;
        } menu;

        base::maybe<rmmr::scene::Root::Id> scene;
        base::maybe<rmmr::scene::Camera::Id> camera;
        base::maybe<rmmr::wrapper::Product::View> view;

        void open(Writing, rmmr::system::Window::Id);
    };

}
