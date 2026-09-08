#pragma once

#include "scenario.h"

namespace eltanin::scenario {

    struct AsterField : Scenario {
        void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) override;
        void populate(Writing, rmmr::system::Device::Id) override;
    };

}
