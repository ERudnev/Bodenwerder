#pragma once

#include <fQSM/api/interface.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>

#include "assets/library.h"

namespace toy {

    using namespace fqsm::api;

    class Demo {
    public:
        struct Handles {
            rmmr::scene::Root::Id scene;
            rmmr::scene::Camera::Id camera;
        };

        virtual ~Demo() = default;

        // Extra catalogue under the same Manager/core; shared Handles as base; before materialize.
        virtual void seedAssets(Writing, rmmr::system::Core::Id, const assets::Handles&) = 0;
        virtual Handles setup(Writing, const assets::Handles&) = 0;
    };

}
