#pragma once

#include "scenario.h"

namespace eltanin::scenario {

    // Map. A locality is opened from here; the other scenarios are locality builders.
    struct Strategic : Scenario {
        void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) override;
        void populate(Writing, rmmr::system::Device::Id) override;
    };

}
