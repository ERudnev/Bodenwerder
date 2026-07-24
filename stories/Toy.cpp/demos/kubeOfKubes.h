#pragma once

#include <base/maybe.h>
#include <rmmr/resources/geometry.q1.h>

#include "demo.h"

namespace toy::demos {

    class KubeOfKubes : public Demo {
    public:
        struct Assets {
            struct {
                base::maybe<rmmr::resource::geometry::Asset::Id> triangle;
                base::maybe<rmmr::resource::geometry::Asset::Id> kube;
                base::maybe<rmmr::resource::geometry::Asset::Id> bagel;
                base::maybe<rmmr::resource::geometry::Asset::Id> grid;
            } primitive;
        };

        Assets assets;

        void seedAssets(Writing, rmmr::system::Core::Id) override;
        Handles setup(Writing, const assets::Handles&) override;
    };

}
