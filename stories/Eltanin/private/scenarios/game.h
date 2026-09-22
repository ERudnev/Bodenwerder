#pragma once

#include "scenario.h"

namespace eltanin::scenario {

    struct Game : Scenario {
        void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) override;
        void populate(Writing, rmmr::system::Device::Id) override;
    };

}
