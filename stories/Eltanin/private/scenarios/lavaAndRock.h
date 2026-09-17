#pragma once

#include "scenario.h"

#include <eltanin/geo/boulder.q1.h>
#include <eltanin/geo/rock.q1.h>

namespace eltanin::scenario {

    struct LavaAndRock : Scenario {
        vector<geo::Rock::Id> rocks;
        vector<geo::Boulder::Id> boulders;

        void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) override;
        void populate(Writing, rmmr::system::Device::Id) override;
    };

}
