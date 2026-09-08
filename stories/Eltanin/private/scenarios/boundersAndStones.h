#pragma once

#include "scenario.h"

#include <eltanin/locality/geo/boulder.q1.h>
#include <eltanin/locality/geo/rock.q1.h>

namespace eltanin::scenario {

    struct BoundersAndStones : Scenario {
        vector<locality::geo::Rock::Id> rocks;
        vector<locality::geo::Boulder::Id> boulders;

        void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) override;
        void populate(Writing, rmmr::system::Device::Id) override;
    };

}
