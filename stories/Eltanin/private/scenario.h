#pragma once

#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/wrapper/library.h>

#include <fQSM/api/interface.h>

namespace eltanin::scenario {

    using namespace fqsm::api;

    struct Scenario {
        virtual void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) = 0;
        virtual void populate(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id) = 0;
    };

}
