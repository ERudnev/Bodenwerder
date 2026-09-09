#pragma once

#include "scenario.h"

#include <rmmr/math.q1.h>

namespace eltanin::scenario {

    struct Planeliod : Scenario {
        static constexpr rmmr::Pos origin{0.0f, 10000.0f, 0.0f};

        void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) override;
        void populate(Writing, rmmr::system::Device::Id) override;
    };

}
