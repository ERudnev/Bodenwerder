#pragma once

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include <base/maybe.h>
#include <base/types/common_types.h>
#include <eltanin/mech/blueprint.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/wrapper/product.h>

#include "blueprints/catalog.h"
#include "fittings/mounts/catalog.h"
#include "views/blueprints/geometry.h"
#include "views/blueprints/history.h"
#include "views/blueprints/membraneSlots.h"
#include "views/blueprints/mountPlacement.h"
#include "views/blueprints/selection.h"

#include <fQSM/api/interface.h>

namespace eltanin::views {

    using namespace fqsm::api;

    // Blueprint editor: catalog + lattice cursor; pick/selection live in blueprints::selection.
    struct Blueprints {
        enum class EditMode : std::uint8_t { skeleton, membranes, mounts };

        struct State {
            struct MainScene {
                base::maybe<rmmr::scene::Root::Id> root;
                base::maybe<rmmr::scene::Camera::Id> camera;
                base::maybe<rmmr::scene::Grid::Id> grid;
                base::maybe<rmmr::scene::actor::Mesh::Id> worldCursor;
                std::vector<blueprints::geometry::QuarkActor> quarkActors;
                std::vector<blueprints::geometry::QuarkActor> clipboardActors;
                std::vector<blueprints::geometry::MountActor> mountActors;
            } mainScene;

            struct PaletteScene {
                base::maybe<rmmr::scene::Root::Id> root;
                base::maybe<rmmr::scene::Camera::Id> camera;
                base::maybe<rmmr::scene::Grid::Id> grid;
                std::vector<blueprints::geometry::PaletteMountActor> actors;
            } paletteScene;

            bool paletteMode;
            EditMode editMode;
            base::maybe<rmmr::resource::meshpack::Asset::Id> interframe;
            base::maybe<::rmmr::resource::material::Asset::Id> ghostMaterial;
            base::maybe<mech::Blueprint::Id> hovered;
            blueprints::geometry::Display display;
            // Catalog mount → Layer from attachment coplanarity (immutable for this run).
            std::unordered_map<mech::Mount::Id, mech::Layer> mountLayers;
            blueprints::selection::Store selection;
            blueprints::history::Store history;
            index3 cursorLattice;
            int currentFloor;
            // Membrane place/remove: candidates from membraneSlots::possible; aim face by mouse ray.
            struct {
                bool enabled;
                base::maybe<std::size_t> cell;
                std::vector<blueprints::membraneSlots::Slot> slots;
                base::maybe<std::size_t> face; // into slots when ray hits a free face
            } membranes;
            blueprints::mountPlacement::Cursor mounts;
            struct {
                bool place;
                bool close;
            } spaceMenu;
        };

        State state;

        void create(Writing);
        void show(Writing, mech::Blueprint::Id);
        void setEditMode(Writing, EditMode);
        void syncGridToFloor(Writing);
        void updateWorldCursor(Writing);
        void syncVisuals(Writing);
        // Layer / spatial filter only — setVisible on resident actors.
        void applyDisplay(Writing);
        void syncClipboardGhost(Writing);
        void syncPalette(Writing, MountCatalog&);
        void rebuildMountLayers(Reading, MountCatalog&);
        void refreshMembraneCandidates(Reading);
        void aimMembraneTarget(Reading);
        void drawMembraneFaceHighlight(Reading) const;
        // Membrane tile mode owns LMB/RMB; does not fall through to selection.
        auto handleMembraneMode(Writing, rmmr::renderer::Integer32 under) -> bool;
        void aimMountCursor(Reading);
        void syncMountCursor(Writing);
        void persistHovered(Writing);
        void applyHistory(Writing, blueprints::history::UiAction);
        void draw(Writing, bool& open, BlueprintCatalog&, MountCatalog&);
        void bindView(std::vector<rmmr::wrapper::Product::View>& views, bool open, const rmmr::wrapper::Product::View& world_view) const;
    };

}
