#pragma once

#include <array>
#include <filesystem>
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
        // Plate has two presentations (both spawned); checkboxes are mutually exclusive (both may be off).
        struct Layers {
            bool plateShape; // levelOne — by topology shape
            bool plateRole;  // levelTwo — by slot role (mesh entry still shape for now)
            bool frame;
            bool inner;
            bool wing;
        };

        struct Actor {
            rmmr::scene::actor::Mesh::Id id;
            mech::layer layer;
            bool plateRole; // meaningful when layer == plate: true = role pack (L2)
        };

        struct State {
            base::maybe<rmmr::scene::Root::Id> scene;
            base::maybe<rmmr::scene::Camera::Id> camera;
            std::array<base::maybe<rmmr::scene::Grid::Id>, 3> grids;
            std::vector<resource::blueprint::Asset::Id> loaded;
            base::maybe<resource::blueprint::Asset::Id> selected;
            Layers layers;
            std::vector<Actor> actors;
        };

        State state;

        void create(Writing, filepath directory);
        void show(Writing, resource::blueprint::Asset::Id);
        auto spawnFrame(Writing, const mech::Element::Cell&, rmmr::resource::meshpack::Asset::Id) -> base::maybe<Actor>;
        // Shape only (role ignored for entry name). Empty / missing → skip. rolePack = levelTwo presentation.
        auto spawnPlate(Writing, const mech::Element::Plate&, rmmr::resource::meshpack::Asset::Id, bool rolePack) -> base::maybe<Actor>;
        void applyLayers(Writing);
        void draw(Writing, bool& open);
        void bindView(std::vector<rmmr::wrapper::Product::View>& views, bool open, const rmmr::wrapper::Product::View& world_view) const;
    };

}
