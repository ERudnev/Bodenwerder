#pragma once

#include "scenario.h"

#include <base/maybe.h>
#include <eltanin/geo/rock.q1.h>
#include <rmmr/resources/texture3array.q1.h>

namespace eltanin::scenario {

    struct AsterField : Scenario {
        struct Assets {
            base::maybe<rmmr::resource::material::Asset::Id> rock;
            base::maybe<rmmr::resource::material::Asset::Id> boulder;
            base::maybe<rmmr::resource::texture3array::Asset::Id> crust;
        };

        Assets assets;

        void loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) override;
        void populate(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id) override;
    };

}
