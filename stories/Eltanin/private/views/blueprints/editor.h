#pragma once

#include <cstddef>
#include <vector>

#include <base/maybe.h>
#include <base/types/common_types.h>
#include <eltanin/resources/blueprint.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/wrapper/product.h>

#include "blueprints/catalog.h"
#include "mech/walls.h"
#include "views/blueprints/geometry.h"
#include "views/blueprints/selection.h"

#include <fQSM/api/interface.h>

namespace eltanin::views {

    using namespace fqsm::api;

    // Blueprint editor: catalog + lattice cursor; pick/selection live in blueprints::selection.
    struct Blueprints {
        struct State {
            base::maybe<rmmr::scene::Root::Id> scene;
            base::maybe<rmmr::scene::Camera::Id> camera;
            base::maybe<rmmr::scene::Grid::Id> grid;
            base::maybe<rmmr::scene::actor::Mesh::Id> worldCursor;
            base::maybe<rmmr::resource::meshpack::Asset::Id> interframe;
            base::maybe<::rmmr::resource::material::Asset::Id> ghostMaterial;
            base::maybe<resource::blueprint::Asset::Id> hovered;
            std::vector<blueprints::geometry::QuarkActor> quarkActors;
            std::vector<blueprints::geometry::QuarkActor> clipboardActors;
            blueprints::geometry::Display display;
            blueprints::selection::Store selection;
            index3 cursorLattice;
            int currentFloor;
            // Modest wall edit: candidates from possibleWalls; aim face by mouse ray (no cycle).
            struct {
                bool enabled;
                base::maybe<std::size_t> cell;
                std::vector<mech::WallSlot> slots;
                base::maybe<std::size_t> face; // into slots when ray hits a free face
            } walls;
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
        void syncVisuals(Writing);
        void syncClipboardGhost(Writing);
        void refreshWallCandidates(Reading);
        void aimWallTarget(Reading);
        void drawWallFaceHighlight(Reading) const;
        // Wall tile mode owns LMB/RMB; does not fall through to selection.
        auto handleWallMode(Writing, rmmr::renderer::Integer32 under) -> bool;
        void persistHovered(Writing);
        void draw(Writing, bool& open, BlueprintCatalog&);
        void bindView(std::vector<rmmr::wrapper::Product::View>& views, bool open, const rmmr::wrapper::Product::View& world_view) const;
    };

}
