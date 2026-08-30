#pragma once

#include "scenario.h"

#include <base/maybe.h>
#include <eltanin/locality/geo/boulder.q1.h>
#include <eltanin/locality/geo/rock.q1.h>
#include <rmmr/resources/texture3array.q1.h>

namespace eltanin::scenario {

    struct LavaAndRock : Scenario {
        struct Assets {
            base::maybe<rmmr::resource::material::Asset::Id> rock;
            base::maybe<rmmr::resource::material::Asset::Id> boulder;
            base::maybe<rmmr::resource::texture3array::Asset::Id> crust;
        };

        Assets assets;
        vector<locality::geo::Rock::Id> rocks;
        vector<locality::geo::Boulder::Id> boulders;

        void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) override;
        void populate(Writing, rmmr::system::Device::Id) override;
    };

}
