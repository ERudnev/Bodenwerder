#pragma once

#include "geo/celestial/planet.h"
#include "scenario.h"

#include <base/maybe.h>

namespace eltanin::scenario {

    struct Planeliod : Scenario {
        void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) override;
        void populate(Writing, rmmr::system::Device::Id) override;
        void placePlanet(Writing, rmmr::system::Device::Id, base::maybe<planet::Planet>&);
    };

}
