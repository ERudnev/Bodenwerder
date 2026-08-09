#pragma once

#include <cstdint>
#include <vector>

#include <base/maybe.h>
#include <base/types/common_types.h>
#include <eltanin/resources/blueprint.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/wrapper/product.h>

#include "blueprints/catalog.h"

#include <fQSM/api/interface.h>

namespace eltanin::views {

    using namespace fqsm::api;

    // Blueprint editor: catalog + lattice cursor + knot actors (identity pick). Space spawns k* → knots.
    struct Blueprints {
        struct KnotActor {
            rmmr::scene::actor::Mesh::Id id;
            std::size_t index; // into Asset.data.knots
        };

        struct State {
            base::maybe<rmmr::scene::Root::Id> scene;
            base::maybe<rmmr::scene::Camera::Id> camera;
            base::maybe<rmmr::scene::Grid::Id> grid;
            base::maybe<rmmr::scene::actor::Mesh::Id> worldCursor;
            base::maybe<resource::blueprint::Asset::Id> hovered;
            base::common_types::index3 cursorLattice;
            int currentFloor;
            std::vector<KnotActor> knotActors;
            std::vector<rmmr::renderer::Integer32> selection;
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
        void clearVisuals(Writing);
        void syncVisuals(Writing);
        void persistHovered(Writing);
        void draw(Writing, bool& open, BlueprintCatalog&);
        void bindView(std::vector<rmmr::wrapper::Product::View>& views, bool open, const rmmr::wrapper::Product::View& world_view) const;
    };

}
