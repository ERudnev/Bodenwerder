#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <base/maybe.h>
#include <eltanin/resources/blueprint.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/wrapper/product.h>

#include "mech/blueprint.h"
#include "mech/semantics/layers.h"

#include <fQSM/api/interface.h>

namespace eltanin::views {

    using namespace fqsm::api;

    // Blueprint viewer: own scene; Game swaps product views when `open`.
    struct Blueprints {
        struct Layers {
            bool plate;
            bool frame;
            bool inner;
            bool wing;
        };

        struct Actor {
            rmmr::scene::actor::Mesh::Id id;
            mech::layer layer;
        };

        struct State {
            base::maybe<rmmr::scene::Root::Id> scene;
            base::maybe<rmmr::scene::Camera::Id> camera;
            std::array<base::maybe<rmmr::scene::Grid::Id>, 3> grids;
            std::vector<resource::blueprint::Asset::Id> loaded;
            base::maybe<resource::blueprint::Asset::Id> selected;
            Layers layers;
            std::vector<Actor> levelOne; // drawn: frame / inner / plate / wing
            std::vector<Actor> levelTwo; // stash; unused for now
        };

        State state;

        void create(Writing, filepath directory);
        void show(Writing, resource::blueprint::Asset::Id);
        auto spawnFromPack(Writing, rmmr::resource::meshpack::Asset::Id, const std::string& entry, const mech::Pose&, mech::layer, rmmr::RGB albedo, float opacity) -> base::maybe<Actor>;
        void applyLayers(Writing);
        void draw(Writing, bool& open);
        void bindView(std::vector<rmmr::wrapper::Product::View>& views, bool open, const rmmr::wrapper::Product::View& world_view) const;
    };

}
