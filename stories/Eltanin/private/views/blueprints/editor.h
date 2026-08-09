#pragma once

#include <vector>

#include <base/maybe.h>
#include <base/types/common_types.h>
#include <eltanin/resources/blueprint.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/wrapper/product.h>

#include "blueprints/catalog.h"

#include <fQSM/api/interface.h>

namespace eltanin::views {

    using namespace fqsm::api;

    // Blueprint editor: catalog + lattice cursor. Space seeds k* → knots/chords in the asset (no mesh actors yet).
    struct Blueprints {
        struct State {
            base::maybe<rmmr::scene::Root::Id> scene;
            base::maybe<rmmr::scene::Camera::Id> camera;
            base::maybe<rmmr::scene::Grid::Id> grid;
            base::maybe<rmmr::scene::actor::Mesh::Id> worldCursor;
            base::maybe<resource::blueprint::Asset::Id> hovered;
            base::common_types::index3 cursorLattice;
            int currentFloor;
            struct {
                bool place;
                bool close;
            } spaceMenu;
        };

        State state;

        void create(Writing);
        void show(Writing, resource::blueprint::Asset::Id);
        void syncGridToFloor(Writing);
        void updateWorldCursor(Writing);
        void persistHovered(Writing);
        void draw(Writing, bool& open, BlueprintCatalog&);
        void bindView(std::vector<rmmr::wrapper::Product::View>& views, bool open, const rmmr::wrapper::Product::View& world_view) const;
    };

}
