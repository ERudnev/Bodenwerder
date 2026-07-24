#pragma once

#include <base/maybe.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/sprites.q1.h>

#include "demo.h"

namespace toy::demos {

    class SpriteTest : public Demo {
    public:
        struct Assets {
            base::maybe<rmmr::resource::sprite::Pack::Id> kenney;
            base::maybe<rmmr::resource::geometry::Asset::Id> unitQuad;
        };

        Assets assets;

        void seedAssets(Writing, rmmr::system::Core::Id, const assets::Handles&) override;
        Handles setup(Writing, const assets::Handles&) override;
    };

}
