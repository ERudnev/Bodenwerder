#pragma once

#include <base/maybe.h>
#include <eltanin/geo/rock.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/wrapper/library.h>

#include <fQSM/api/interface.h>

namespace eltanin::scenarios {

    using namespace fqsm::api;

    struct BoundersAndStones {
        struct Assets {
            base::maybe<rmmr::resource::material::Asset::Id> rock;
            base::maybe<rmmr::resource::material::Asset::Id> boulder;
            base::maybe<rmmr::resource::texture3array::Asset::Id> crust;
        };

        Assets assets;
        vector<geo::Rock::Id> rocks;

        auto loadResources(Writing, const rmmr::wrapper::assets::Handles& shared) -> bool;
        void populate(Writing, rmmr::scene::Root::Id, rmmr::system::Device::Id);
    };

}
