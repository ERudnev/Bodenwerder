#pragma once

#include "views/starMap/ui.h"
#include "views/starMap/visuals.h"

#include <base/maybe.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/system/viewInput.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/wrapper/product.h>

#include <fQSM/api/interface.h>

namespace eltanin::views::starmap {

    using namespace fqsm::api;

    struct View {
        Menu menu;
        base::maybe<rmmr::scene::Root::Id> scene;
        base::maybe<rmmr::scene::Camera::Id> camera;
        base::maybe<rmmr::system::ViewInput::Id> input;
        base::maybe<rmmr::wrapper::Product::View> view;
        Visuals visuals;

        void open(Writing, rmmr::system::Window::Id);
        void follow(Writing);
    };

}
