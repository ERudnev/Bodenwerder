#pragma once

#include "scenario.h"

#include <rmmr/math.q1.h>

namespace eltanin::scenario {

    struct Planeliod : Scenario {
        // Surface reference above planetoid at origin with radius 10 km (north pole).
        static constexpr rmmr::Pos origin{10000.0f, 0.0f, 0.0f};

        void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) override;
        void populate(Writing, rmmr::system::Device::Id) override;
    };

}
