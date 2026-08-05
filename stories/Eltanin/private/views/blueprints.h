#pragma once

#include <array>
#include <filesystem>
#include <vector>

#include <base/maybe.h>
#include <eltanin/resources/blueprint.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/wrapper/product.h>

#include <fQSM/api/interface.h>

namespace eltanin::views {

    using namespace fqsm::api;

    // Blueprint viewer: own scene; Game swaps product views when `open`.
    struct Blueprints {
        struct State {
            base::maybe<rmmr::scene::Root::Id> scene;
            base::maybe<rmmr::scene::Camera::Id> camera;
            std::array<base::maybe<rmmr::scene::Grid::Id>, 3> grids;
            std::vector<resource::blueprint::Asset::Id> loaded;
            base::maybe<resource::blueprint::Asset::Id> selected;
        };

        State state;

        void create(Writing, filepath directory);
        void draw(Reading, bool& open);
        void bindView(std::vector<rmmr::wrapper::Product::View>& views, bool open, const rmmr::wrapper::Product::View& world_view) const;
    };

}
